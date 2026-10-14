#define GLM_ENABLE_EXPERIMENTAL
#include "renderer.h"
#include "texture_utils.h"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <glm/ext.hpp>
#include <glm/gtx/exterior_product.hpp>

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
             glm::vec4(glm::vec3(frame.model * glm::vec4(position, 1.0f)), 1.0f),
             glm::vec4(glm::normalize(normalTransform * normal), 0.0f),
             glm::vec4(vertex.uv, 0.0f, 0.0f)});
    }
    return vertices;
}

static TransformedVertex interpolate(TransformedVertex const& a, TransformedVertex const& b, float t) {
    return {
        glm::mix(a.clip, b.clip, t),
        glm::mix(a.worldPosition, b.worldPosition, t),
        glm::vec4(glm::normalize(glm::mix(glm::vec3(a.normal), glm::vec3(b.normal), t)), 0.0f),
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

std::vector<PreparedTriangle>
Renderer::prepareTriangles(Mesh const& mesh, glm::mat4 const& transform, FrameState const& frame) {
    auto const vertices = transformVertices(mesh, transform, frame);
    return prepareTriangles(vertices, mesh.indices(), frame.extent);
}

std::vector<PreparedTriangle> Renderer::prepareTriangles(std::vector<TransformedVertex> const& vertices,
                                                         std::vector<uint32_t> const& indices,
                                                         glm::ivec2 extent) {
    std::vector<PreparedTriangle> result;
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
            PreparedTriangle output{};
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
            glm::vec3 const edge1 = glm::vec3(clipped[1].worldPosition - clipped[0].worldPosition);
            glm::vec3 const edge2 = glm::vec3(clipped[2].worldPosition - clipped[0].worldPosition);
            glm::vec2 const deltaUv1 = glm::vec2(clipped[1].uv - clipped[0].uv);
            glm::vec2 const deltaUv2 = glm::vec2(clipped[2].uv - clipped[0].uv);
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
