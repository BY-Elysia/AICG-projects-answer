#define GLM_ENABLE_EXPERIMENTAL
#include "cpu_renderer.h"

#include <cmath>
#include <stdexcept>
#include <glm/gtx/exterior_product.hpp>

#include "pbr_shading.h"

// CPU geometry-stage triangle with the attributes needed for software rasterization.
struct ScreenVertex {
    // xy is pixel-space position, z is normalized depth, and w is 1 / clip-space w.
    glm::vec4 screenAndInvW{0.0f};
    // World-space position and unnormalized normal to interpolate for lighting.
    glm::vec3 worldPosition{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
};

struct ScreenTriangle {
    // Clockwise/back-facing triangles are removed before this stage.
    ScreenVertex vertices[3];
    // UV-derived world-space T basis for normal mapping.
    glm::vec3 tangent{1.0f, 0.0f, 0.0f};
    // UV-derived world-space B basis for normal mapping.
    glm::vec3 bitangent{0.0f, 1.0f, 0.0f};
};

// Transform every input vertex once, then assemble indexed triangles from its varyings.
static std::vector<TransformedVertex>
transformVertices(Mesh const& mesh, glm::mat4 const& transform, FrameState const& frame) {
    std::vector<TransformedVertex> vertices;
    vertices.reserve(mesh.vertices().size());
    glm::mat3 const nodeNormalTransform = glm::transpose(glm::inverse(glm::mat3(transform)));
    glm::mat3 const normalTransform = glm::transpose(glm::inverse(glm::mat3(frame.model)));
    glm::mat4 const modelViewProjection = frame.projection * frame.view * frame.model;
    for (auto const& vertex : mesh.vertices()) {
        // Preserve the asset-node then instance-transform order for positions and normals.
        glm::vec3 const position = glm::vec3(transform * glm::vec4(vertex.position, 1.0f));
        glm::vec3 const normal = glm::normalize(nodeNormalTransform * vertex.normal);
        vertices.push_back(
            {modelViewProjection * glm::vec4(position, 1.0f),
             glm::vec3(frame.model * glm::vec4(position, 1.0f)),
             glm::normalize(normalTransform * normal),
             vertex.uv});
    }

    return vertices;
}

static TransformedVertex interpolate(TransformedVertex const& a, TransformedVertex const& b, float t) {
    return {
        glm::mix(a.clip, b.clip, t),
        glm::mix(a.worldPosition, b.worldPosition, t),
        glm::normalize(glm::mix(a.normal, b.normal, t)),
        glm::mix(a.uv, b.uv, t)
    };
}

template <typename Distance> static void clipAgainst(std::vector<TransformedVertex>& polygon, Distance distance) {
    std::vector<TransformedVertex> result;
    if (polygon.empty()) {
        return;
    }
    TransformedVertex previous = polygon.back();
    float previousDistance = distance(previous);
    for (TransformedVertex const& current : polygon) {
        float const currentDistance = distance(current);
        bool const currentInside = currentDistance >= 0.0f;
        bool const previousInside = previousDistance >= 0.0f;
        if (currentInside != previousInside) {
            float const t = previousDistance / (previousDistance - currentDistance);
            result.push_back(interpolate(previous, current, t));
        }
        if (currentInside) {
            result.push_back(current);
        }
        previous = current;
        previousDistance = currentDistance;
    }
    polygon = std::move(result);
}

static std::vector<ScreenTriangle> buildScreenTriangles(std::vector<TransformedVertex> const& vertices,
                                                        std::vector<uint32_t> const& indices,
                                                        glm::ivec2 extent) {
    std::vector<ScreenTriangle> result;
    result.reserve(indices.size() / 3);
    if (indices.size() % 3 != 0) {
        throw std::invalid_argument("Mesh indices must describe complete triangles");
    }
    for (size_t index = 0; index + 2 < indices.size(); index += 3) {
        std::vector<TransformedVertex> polygon;
        polygon.reserve(8);
        for (int corner = 0; corner < 3; ++corner) {
            auto const vertexIndex = indices[index + corner];
            if (vertexIndex >= vertices.size()) {
                throw std::invalid_argument("Mesh index references a missing vertex");
            }
            polygon.push_back(vertices[vertexIndex]);
        }
        clipAgainst(polygon, [](TransformedVertex const& v) { return v.clip.x + v.clip.w; });
        clipAgainst(polygon, [](TransformedVertex const& v) { return v.clip.w - v.clip.x; });
        clipAgainst(polygon, [](TransformedVertex const& v) { return v.clip.y + v.clip.w; });
        clipAgainst(polygon, [](TransformedVertex const& v) { return v.clip.w - v.clip.y; });
        clipAgainst(polygon, [](TransformedVertex const& v) { return v.clip.z + v.clip.w; });
        clipAgainst(polygon, [](TransformedVertex const& v) { return v.clip.w - v.clip.z; });
        for (size_t triangle = 1; triangle + 1 < polygon.size(); ++triangle) {
            TransformedVertex const clipped[] = {polygon[0], polygon[triangle], polygon[triangle + 1]};
            ScreenTriangle output{};
            for (int corner = 0; corner < 3; ++corner) {
                float const reciprocalW = 1.0f / clipped[corner].clip.w;
                glm::vec3 const ndc = glm::vec3(clipped[corner].clip) * reciprocalW;
                output.vertices[corner].screenAndInvW = glm::vec4((ndc.x * 0.5f + 0.5f) * extent.x,
                                                                  (ndc.y * 0.5f + 0.5f) * extent.y,
                                                                  ndc.z * 0.5f + 0.5f,
                                                                  reciprocalW);
                output.vertices[corner].worldPosition = clipped[corner].worldPosition;
                output.vertices[corner].normal = clipped[corner].normal;
                output.vertices[corner].uv = clipped[corner].uv;
            }
            glm::vec3 const edge1 = clipped[1].worldPosition - clipped[0].worldPosition;
            glm::vec3 const edge2 = clipped[2].worldPosition - clipped[0].worldPosition;
            glm::vec2 const deltaUv1 = clipped[1].uv - clipped[0].uv;
            glm::vec2 const deltaUv2 = clipped[2].uv - clipped[0].uv;
            float const determinant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
            if (std::abs(determinant) > 1e-8f) {
                output.tangent = glm::normalize((edge1 * deltaUv2.y - edge2 * deltaUv1.y) / determinant);
                output.bitangent = glm::normalize((-edge1 * deltaUv2.x + edge2 * deltaUv1.x) / determinant);
            }
            glm::vec2 const a(output.vertices[0].screenAndInvW);
            glm::vec2 const b(output.vertices[1].screenAndInvW);
            glm::vec2 const c(output.vertices[2].screenAndInvW);
            if (glm::cross(b - a, c - a) > 0.0f) {
                result.push_back(output);
            }
        }
    }
    return result;
}

CpuRenderer::CpuRenderer(uint32_t width, uint32_t height)
    : Renderer(width, height) {
    resizeBackend();
}

void CpuRenderer::resizeBackend() {
    // Keep the color and depth buffers across all draw items in a frame.
    size_t const pixels = static_cast<size_t>(m_extent.x) * m_extent.y;

    m_color.resize(pixels);
    m_depthBuffer.resize(pixels);
}

void CpuRenderer::render(std::vector<DrawItem> const& drawItems, FrameState const& frame) {
    // Clear the buffers for the next frame.
    std::fill(m_color.begin(), m_color.end(), glm::vec4(0.025f, 0.03f, 0.04f, 1.0f));
    std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), 1.0f);

    for (DrawItem const& drawItem : drawItems) {
        // Transform and assemble this draw item's indexed triangles.
        auto const vertices = transformVertices(*drawItem.mesh, drawItem.transform, frame);
        auto const triangles = buildScreenTriangles(vertices, drawItem.mesh->indices(), frame.extent);

        // Rasterize and shade before moving to the next draw item.
        for (ScreenTriangle const& triangle : triangles) {
            glm::vec2 const a(triangle.vertices[0].screenAndInvW);
            glm::vec2 const b(triangle.vertices[1].screenAndInvW);
            glm::vec2 const c(triangle.vertices[2].screenAndInvW);
            float const area = glm::cross(b - a, c - a);
            if (area <= 0.0f) {
                continue;
            }

            // Restrict coverage tests to the triangle's screen-space bounding box.
            glm::vec2 const minPoint = glm::min(a, glm::min(b, c));
            glm::vec2 const maxPoint = glm::max(a, glm::max(b, c));
            int const minX = std::max(0, static_cast<int>(std::floor(minPoint.x)));
            int const minY = std::max(0, static_cast<int>(std::floor(minPoint.y)));
            int const maxX = std::min(m_extent.x - 1, static_cast<int>(std::ceil(maxPoint.x)));
            int const maxY = std::min(m_extent.y - 1, static_cast<int>(std::ceil(maxPoint.y)));
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    glm::vec2 const sample(float(x) + 0.5f, float(y) + 0.5f);
                    // Cross products test pixel-center coverage and produce barycentric coordinates.
                    glm::vec3 barycentric(
                        glm::cross(c - b, sample - b), glm::cross(a - c, sample - c), glm::cross(b - a, sample - a));
                    if (barycentric.x < 0.0f || barycentric.y < 0.0f || barycentric.z < 0.0f) {
                        continue;
                    }
                    barycentric /= area;
                    float const depth = glm::dot(barycentric,
                                                 glm::vec3(triangle.vertices[0].screenAndInvW.z,
                                                           triangle.vertices[1].screenAndInvW.z,
                                                           triangle.vertices[2].screenAndInvW.z));
                    size_t const pixelIndex = static_cast<size_t>(y) * m_extent.x + x;
                    if (depth >= m_depthBuffer[pixelIndex]) {
                        continue;
                    }

                    m_depthBuffer[pixelIndex] = depth;
                    // Reweight by 1/w and normalize to interpolate attributes with perspective
                    // correction.
                    glm::vec3 const corrected = barycentric * glm::vec3(triangle.vertices[0].screenAndInvW.w,
                                                                        triangle.vertices[1].screenAndInvW.w,
                                                                        triangle.vertices[2].screenAndInvW.w);
                    glm::vec3 const weights = corrected / (corrected.x + corrected.y + corrected.z);
                    glm::vec3 position(0.0f), normal(0.0f);
                    glm::vec2 uv(0.0f);
                    for (int corner = 0; corner < 3; ++corner) {
                        position += weights[corner] * triangle.vertices[corner].worldPosition;
                        normal += weights[corner] * triangle.vertices[corner].normal;
                        uv += weights[corner] * triangle.vertices[corner].uv;
                    }
                    // Shade the visible sample with material textures and direct-light PBR.
                    m_color[pixelIndex] = glm::vec4(
                        pbr::shade(
                            *drawItem.material, frame, position, normal, triangle.tangent, triangle.bitangent, uv),
                        1.0f);
                }
            }
        }
    }

    // Upload the CPU result so the same full-screen pass can present either renderer.
    m_outputTexture->update(m_color.data(), gfx::Texture::ExternalFormat::RGBA, gfx::Texture::PixelType::Float);
}
