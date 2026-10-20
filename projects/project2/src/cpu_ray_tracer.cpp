#include "cpu_ray_tracer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

constexpr float kEpsilon = 1e-3f;
constexpr int kMaxTraceDepth = 16;

static float randomFloat(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return std::min(0.99999994f, static_cast<float>(state) * (1.0f / 4294967296.0f));
}

static glm::vec3 cosineHemisphere(uint32_t& state) {
    auto const u = randomFloat(state);
    auto const phi = 2.0f * std::numbers::pi_v<float> * randomFloat(state);
    auto const radius = std::sqrt(u);
    return {std::cos(phi) * radius, std::sin(phi) * radius, std::sqrt(1.0f - u)};
}

static glm::vec3 toWorld(glm::vec3 normal, glm::vec3 sample) {
    normal = glm::normalize(normal);
    auto const tangent = glm::normalize(std::abs(normal.x) > std::abs(normal.y) ? glm::vec3{-normal.z, 0.0f, normal.x}
                                                                                : glm::vec3{0.0f, -normal.z, normal.y});
    auto const bitangent = glm::cross(normal, tangent);
    return sample.x * tangent + sample.y * bitangent + sample.z * normal;
}

static bool intersects(AABB const& box, Ray const& ray) {
    float near = 0.0f;
    float far = ray.tMax;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(ray.direction[axis]) < 1e-8f) {
            if (ray.origin[axis] < box.min[axis] || ray.origin[axis] > box.max[axis]) {
                return false;
            }
            continue;
        }
        auto first = (box.min[axis] - ray.origin[axis]) / ray.direction[axis];
        auto second = (box.max[axis] - ray.origin[axis]) / ray.direction[axis];
        if (first > second) {
            std::swap(first, second);
        }
        near = std::max(near, first);
        far = std::min(far, second);
        if (near > far) {
            return false;
        }
    }
    return true;
}

CpuRayTracer::CpuRayTracer(uint32_t width, uint32_t height)
    : RayTracer{width, height} {
    onResize();
    reset();
}

void CpuRayTracer::onResize() {
    auto const count = static_cast<size_t>(m_width) * m_height;
    m_rngStates.resize(count);
    for (size_t i = 0; i < m_rngStates.size(); ++i) {
        m_rngStates[i] = 1664525u * static_cast<uint32_t>(i) + 1013904223u;
    }

    m_average.assign(count, glm::vec3{0.0f});
    m_frame = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = m_width,
            .height = m_height,
            .format = gfx::Texture::Format::RGB32F,
            .pixelFormat = gfx::Texture::ExternalFormat::RGB,
            .pixelType = gfx::Texture::PixelType::Float
        },
        m_average.data());
}

void CpuRayTracer::onReset() {
    std::fill(m_average.begin(), m_average.end(), glm::vec3{0.0f});
}

void CpuRayTracer::render(glm::mat4 const& cameraToWorld, glm::mat4 const& rasterToCamera) {
    if (m_average.empty() || !m_scene || m_scene->nodes().empty()) {
        return;
    }

    auto const cameraOrigin = glm::vec3(cameraToWorld * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});
    auto const sampleWeight = 1.0f / static_cast<float>(m_completedSamples + 1);
    for (size_t index = 0; index < m_average.size(); ++index) {
        auto& state = m_rngStates[index];
        auto const x = static_cast<float>(index % m_width) + randomFloat(state);
        auto const y = static_cast<float>(index / m_width) + randomFloat(state);
        auto const cameraDirection = glm::vec3(rasterToCamera * glm::vec4{x, y, 0.0f, 1.0f});
        Ray ray{cameraOrigin, glm::normalize(glm::vec3(cameraToWorld * glm::vec4(cameraDirection, 0.0f)))};

        auto& average = m_average[index];
        average += (trace(ray, state) - average) * sampleWeight;
    }
    ++m_completedSamples;
    m_frame->update(m_average.data(), gfx::Texture::ExternalFormat::RGB, gfx::Texture::PixelType::Float);
}

bool CpuRayTracer::intersectPrimitive(Ray& ray, Primitive const& primitive, Hit& hit) const {
    if (primitive.type == Primitive::Type::Sphere) {
        auto const& sphere = m_scene->spheres()[primitive.shapeIndex];
        auto const offset = ray.origin - sphere.position;
        auto const halfB = glm::dot(offset, ray.direction);
        auto const c = glm::dot(offset, offset) - sphere.radius * sphere.radius;
        auto const discriminant = halfB * halfB - c;
        if (discriminant < 0.0f) {
            return false;
        }
        auto const root = std::sqrt(discriminant);
        auto t = -halfB - root;
        if (t <= kEpsilon) {
            t = -halfB + root;
        }
        if (t <= kEpsilon || t >= ray.tMax) {
            return false;
        }
        ray.tMax = t;
        hit.position = ray.at(t);
        hit.normal = glm::normalize(hit.position - sphere.position);
    }
    else {
        auto const& triangle = m_scene->triangles()[primitive.shapeIndex];
        auto const& a = m_scene->vertices()[triangle.v[0]];
        auto const& b = m_scene->vertices()[triangle.v[1]];
        auto const& c = m_scene->vertices()[triangle.v[2]];
        auto const edge1 = b.position - a.position;
        auto const edge2 = c.position - a.position;
        auto const p = glm::cross(ray.direction, edge2);
        auto const determinant = glm::dot(edge1, p);
        if (std::abs(determinant) < 1e-8f) {
            return false;
        }
        auto const inverseDeterminant = 1.0f / determinant;
        auto const offset = ray.origin - a.position;
        auto const beta = glm::dot(offset, p) * inverseDeterminant;
        if (beta < 0.0f || beta > 1.0f) {
            return false;
        }
        auto const q = glm::cross(offset, edge1);
        auto const gamma = glm::dot(ray.direction, q) * inverseDeterminant;
        if (gamma < 0.0f || beta + gamma > 1.0f) {
            return false;
        }
        auto const t = glm::dot(edge2, q) * inverseDeterminant;
        if (t <= kEpsilon || t >= ray.tMax) {
            return false;
        }
        ray.tMax = t;
        hit.position = ray.at(t);
        auto const interpolatedNormal = (1.0f - beta - gamma) * a.normal + beta * b.normal + gamma * c.normal;
        hit.normal = glm::normalize(glm::dot(interpolatedNormal, interpolatedNormal) > 0.0f ? interpolatedNormal
                                                                                            : glm::cross(edge1, edge2));
    }

    hit.materialIndex = primitive.materialIndex;
    return true;
}

bool CpuRayTracer::intersect(Ray& ray, Hit& hit) const {
    std::array<int, 64> stack{};
    size_t stackSize = 0;
    int nodeIndex = 0;
    bool found = false;

    while (true) {
        auto const& node = m_scene->nodes()[nodeIndex];
        if (intersects(node.bounds, ray)) {
            if (node.isLeaf()) {
                for (int i = 0; i < -node.second; ++i) {
                    found |= intersectPrimitive(ray, m_scene->primitives()[node.first + i], hit);
                }
            }
            else {
                stack[stackSize++] = node.second;
                nodeIndex = node.first;
                continue;
            }
        }
        if (stackSize == 0) {
            break;
        }
        nodeIndex = stack[--stackSize];
    }
    return found;
}

glm::vec3 CpuRayTracer::trace(Ray ray, uint32_t& rngState) const {
    glm::vec3 color{1.0f};
    for (int step = 0; step < kMaxTraceDepth; ++step) {
        Hit hit;
        if (!intersect(ray, hit)) {
            return color * (m_skybox ? m_skybox->sample(ray.direction) : glm::vec3{0.0f});
        }
        auto const& material = m_scene->materials()[hit.materialIndex];
        color *= material.albedo;
        auto const normal = glm::normalize(hit.normal);

        if (material.type == Material::Type::Lambertian) {
            ray.origin = hit.position + kEpsilon * normal;
            ray.direction = glm::normalize(toWorld(normal, cosineHemisphere(rngState)));
            if (glm::dot(ray.direction, normal) <= 0.0f) {
                break;
            }
        }
        else if (material.type == Material::Type::Metal) {
            auto const reflection = glm::reflect(ray.direction, normal);
            ray.origin = hit.position + kEpsilon * normal;
            ray.direction =
                glm::normalize(reflection + material.fuzz * toWorld(reflection, cosineHemisphere(rngState)));
            if (glm::dot(ray.direction, normal) <= 0.0f) {
                break;
            }
        }
        else {
            auto facingNormal = normal;
            auto const cosine = glm::dot(ray.direction, normal);
            auto const eta = cosine > 0.0f ? material.ior : 1.0f / material.ior;
            if (cosine > 0.0f) {
                facingNormal = -normal;
            }
            auto const refraction = glm::refract(ray.direction, facingNormal, eta);
            auto const r0 = (1.0f - material.ior) / (1.0f + material.ior);
            auto const fresnel =
                r0 * r0 +
                (1.0f - r0 * r0) *
                    std::pow(1.0f - std::clamp(cosine > 0.0f ? material.ior * cosine : -cosine, 0.0f, 1.0f), 5.0f);
            if (glm::dot(refraction, refraction) == 0.0f || fresnel > randomFloat(rngState)) {
                ray.origin = hit.position + kEpsilon * facingNormal;
                ray.direction = glm::normalize(glm::reflect(ray.direction, facingNormal));
            }
            else {
                ray.origin = hit.position - kEpsilon * facingNormal;
                ray.direction = glm::normalize(refraction);
            }
        }
        ray.tMax = std::numeric_limits<float>::max();
    }
    return color;
}
