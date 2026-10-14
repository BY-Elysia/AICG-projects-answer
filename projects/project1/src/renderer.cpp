#define GLM_ENABLE_EXPERIMENTAL
#include "renderer.h"
#include "texture_utils.h"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <glm/ext.hpp>
#include <glm/gtx/exterior_product.hpp>

namespace {
struct ClipVertex {
    glm::vec4 clip;
    glm::vec3 worldPosition;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Transform every input vertex once, then assemble indexed triangles from its varyings.
std::vector<ClipVertex> transformVertices(Mesh const& mesh, FrameState const& frame) {
    std::vector<ClipVertex> vertices;
    vertices.reserve(mesh.vertices.size());
    glm::mat3 const nodeNormalTransform = glm::transpose(glm::inverse(glm::mat3(mesh.transform)));
    glm::mat3 const normalTransform = glm::transpose(glm::inverse(glm::mat3(frame.model)));
    glm::mat4 const modelViewProjection = frame.projection * frame.view * frame.model;
    for (auto const& vertex : mesh.vertices) {
        // Preserve the asset-node then instance-transform order for positions and normals.
        glm::vec3 const position = glm::vec3(mesh.transform * glm::vec4(vertex.position, 1.0f));
        glm::vec3 const normal = glm::normalize(nodeNormalTransform * vertex.normal);
        vertices.push_back(
            {modelViewProjection * glm::vec4(position, 1.0f),
             glm::vec3(frame.model * glm::vec4(position, 1.0f)),
             glm::normalize(normalTransform * normal),
             vertex.uv});
    }
    return vertices;
}

ClipVertex interpolate(ClipVertex const& a, ClipVertex const& b, float t) {
    return {
        glm::mix(a.clip, b.clip, t),
        glm::mix(a.worldPosition, b.worldPosition, t),
        glm::normalize(glm::mix(a.normal, b.normal, t)),
        glm::mix(a.uv, b.uv, t)
    };
}

template <typename Distance> void clipAgainst(std::vector<ClipVertex>& polygon, Distance distance) {
    std::vector<ClipVertex> result;
    if (polygon.empty()) {
        return;
    }
    ClipVertex previous = polygon.back();
    float previousDistance = distance(previous);
    for (ClipVertex const& current : polygon) {
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
} // namespace

void Renderer::resize(glm::ivec2 const& extent) {
    if (extent.x <= 0 || extent.y <= 0) {
        throw std::invalid_argument("Render extent must be positive");
    }
    if (extent == m_extent) {
        return;
    }
    m_extent = extent;
    m_outputTexture = makeRasterTexture(extent, gfx::Texture::Format::RGBA16F, gfx::Texture::ExternalFormat::RGBA);
    resizeBackend();
}

std::vector<PreparedTriangle> Renderer::prepareTriangles(Mesh const& mesh, FrameState const& frame) {
    std::vector<PreparedTriangle> result;
    result.reserve(mesh.indices.size() / 3);
    if (mesh.indices.size() % 3 != 0) {
        throw std::invalid_argument("Mesh indices must describe complete triangles");
    }
    auto const vertices = transformVertices(mesh, frame);
    for (size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
        std::vector<ClipVertex> polygon;
        polygon.reserve(8);
        for (int corner = 0; corner < 3; ++corner) {
            auto const vertexIndex = mesh.indices[index + corner];
            if (vertexIndex >= vertices.size()) {
                throw std::invalid_argument("Mesh index references a missing vertex");
            }
            polygon.push_back(vertices[vertexIndex]);
        }
        clipAgainst(polygon, [](ClipVertex const& v) { return v.clip.x + v.clip.w; });
        clipAgainst(polygon, [](ClipVertex const& v) { return v.clip.w - v.clip.x; });
        clipAgainst(polygon, [](ClipVertex const& v) { return v.clip.y + v.clip.w; });
        clipAgainst(polygon, [](ClipVertex const& v) { return v.clip.w - v.clip.y; });
        clipAgainst(polygon, [](ClipVertex const& v) { return v.clip.z + v.clip.w; });
        clipAgainst(polygon, [](ClipVertex const& v) { return v.clip.w - v.clip.z; });
        for (size_t triangle = 1; triangle + 1 < polygon.size(); ++triangle) {
            ClipVertex const clipped[] = {polygon[0], polygon[triangle], polygon[triangle + 1]};
            PreparedTriangle output{};
            for (int corner = 0; corner < 3; ++corner) {
                float const reciprocalW = 1.0f / clipped[corner].clip.w;
                glm::vec3 const ndc = glm::vec3(clipped[corner].clip) * reciprocalW;
                output.vertices[corner].screenAndInvW = glm::vec4((ndc.x * 0.5f + 0.5f) * frame.extent.x,
                                                                  (ndc.y * 0.5f + 0.5f) * frame.extent.y,
                                                                  ndc.z * 0.5f + 0.5f,
                                                                  reciprocalW);
                output.vertices[corner].worldPosition = glm::vec4(clipped[corner].worldPosition, 1.0f);
                output.vertices[corner].normal = glm::vec4(clipped[corner].normal, 0.0f);
                output.vertices[corner].uv = glm::vec4(clipped[corner].uv, 0.0f, 0.0f);
            }
            glm::vec3 const edge1 = clipped[1].worldPosition - clipped[0].worldPosition;
            glm::vec3 const edge2 = clipped[2].worldPosition - clipped[0].worldPosition;
            glm::vec2 const deltaUv1 = clipped[1].uv - clipped[0].uv;
            glm::vec2 const deltaUv2 = clipped[2].uv - clipped[0].uv;
            float const determinant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
            if (std::abs(determinant) > 1e-8f) {
                output.tangent =
                    glm::vec4(glm::normalize((edge1 * deltaUv2.y - edge2 * deltaUv1.y) / determinant), 0.0f);
                output.bitangent =
                    glm::vec4(glm::normalize((-edge1 * deltaUv2.x + edge2 * deltaUv1.x) / determinant), 0.0f);
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
