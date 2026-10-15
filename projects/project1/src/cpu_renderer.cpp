#define GLM_ENABLE_EXPERIMENTAL
#include "cpu_renderer.h"

#include <array>
#include <cmath>
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

// A clipping plane can cross at most two edges of a convex polygon. It adds two
// intersection vertices but removes at least one old vertex, so the count grows
// by at most one per plane: 3 triangle vertices + 6 clip planes = 9.
constexpr size_t kMaxClippedPolygonVertices = 3 + 6;

// Transform each unique mesh vertex once; indexed triangles reuse these results later.
static std::vector<TransformedVertex> transformVertices(Mesh const& mesh, FrameState const& frame) {
    std::vector<TransformedVertex> vertices;
    vertices.reserve(mesh.vertices().size());

    // The app has already combined the scene and node transforms in frame.model.
    // Use its inverse transpose for normals under non-uniform scaling.
    glm::mat3 const normalTransform = glm::transpose(glm::inverse(glm::mat3(frame.model)));
    glm::mat4 const modelViewProjection = frame.projection * frame.view * frame.model;

    for (auto const& vertex : mesh.vertices()) {
        glm::vec4 const position(vertex.position, 1.0f);
        glm::vec4 const clip = modelViewProjection * position;
        glm::vec3 const worldPosition = glm::vec3(frame.model * position);
        glm::vec3 const worldNormal = glm::normalize(normalTransform * vertex.normal);

        // UVs belong to the mesh surface, so geometric transforms leave them unchanged.
        vertices.push_back({clip, worldPosition, worldNormal, vertex.uv});
    }

    return vertices;
}

// Clip a polygon against one plane in homogeneous clip space. For
// plane = (a, b, c, d), the boundary is a*x + b*y + c*z + d*w = 0.
// Keep the half-space where dot(plane, vertex.clip) >= 0; the other side is outside.
// polygon and count are updated in place; scratch storage keeps input edges intact.
static void
clipAgainst(std::array<TransformedVertex, kMaxClippedPolygonVertices>& polygon, size_t& count, glm::vec4 const& plane) {

    // Perform Sutherland-Hodgeman algorithm to clip the triangle with a single plane
    // Hint: use an additional array clipped to storge the clipped results
    // Hint: copy the clipped results to polygon as the output

    /**************************************** Todo Begin *****************************************/
    if (count != 0) {
        std::array<TransformedVertex, kMaxClippedPolygonVertices> clipped;
        size_t clippedCount = 0;
        // Walk every edge, including the closing edge from the last vertex to the first.
        TransformedVertex previous = polygon[count - 1];
        float previousSide = glm::dot(plane, previous.clip);
        for (size_t i = 0; i < count; ++i) {
            TransformedVertex const& current = polygon[i];
            float const currentSide = glm::dot(plane, current.clip);
            bool const currentInside = currentSide >= 0.0f;
            bool const previousInside = previousSide >= 0.0f;

            // Inside -> outside: keep the crossing. Outside -> inside: keep the
            // crossing and the current vertex. Both inside: keep just the current
            // vertex. Both outside: keep neither.
            if (currentInside != previousInside) {
                // The side value varies linearly along the edge; solve side(t) = 0.
                float const t = previousSide / (previousSide - currentSide);
                clipped[clippedCount++] = {
                    glm::mix(previous.clip, current.clip, t),
                    glm::mix(previous.worldPosition, current.worldPosition, t),
                    glm::normalize(glm::mix(previous.normal, current.normal, t)),
                    glm::mix(previous.uv, current.uv, t)
                };
            }
            if (currentInside) {
                clipped[clippedCount++] = current;
            }
            previous = current;
            previousSide = currentSide;
        }
        count = clippedCount;
        for (size_t i = 0; i < count; ++i) {
            polygon[i] = clipped[i];
        }
    }

    /***************************************** Todo End ******************************************/
}

// Assemble indexed clip-space vertices into clipped, front-facing screen triangles.
static std::vector<ScreenTriangle> buildScreenTriangles(std::vector<TransformedVertex> const& vertices,
                                                        std::vector<uint32_t> const& indices,
                                                        glm::uvec2 extent) {
    std::vector<ScreenTriangle> result;
    result.reserve(indices.size() / 3);

    // Each consecutive group of three indices names one source triangle.
    for (size_t index = 0; index < indices.size(); index += 3) {
        // Fixed storage covers the worst-case clipped polygon. Only the first
        // count entries are valid; initially they are the source triangle.
        std::array<TransformedVertex, kMaxClippedPolygonVertices> polygon;
        size_t count = 3;
        for (int corner = 0; corner < 3; ++corner) {
            auto const vertexIndex = indices[index + corner];
            polygon[corner] = vertices[vertexIndex];
        }

        // Perform Sutherland-Hodgeman algorithm to clip the triangle with 6 planes

        // OpenGL's homogeneous clip volume is -w <= x, y, z <= w.
        // A nonnegative result means the vertex is inside the corresponding plane.
        // Clipping may discard the triangle or add intersection vertices.

        /************************************** Todo Begin ***************************************/

        clipAgainst(polygon, count, {1.0f, 0.0f, 0.0f, 1.0f});  // left: x + w >= 0
        clipAgainst(polygon, count, {-1.0f, 0.0f, 0.0f, 1.0f}); // right: w - x >= 0
        clipAgainst(polygon, count, {0.0f, 1.0f, 0.0f, 1.0f});  // bottom: y + w >= 0
        clipAgainst(polygon, count, {0.0f, -1.0f, 0.0f, 1.0f}); // top: w - y >= 0
        clipAgainst(polygon, count, {0.0f, 0.0f, 1.0f, 1.0f});  // near: z + w >= 0
        clipAgainst(polygon, count, {0.0f, 0.0f, -1.0f, 1.0f}); // far: w - z >= 0

        /*************************************** Todo End ****************************************/

        // TODO: For each clipped triangle, calculate the following attribute for its screen triangle

        // The clipped polygon is convex. Fan it out as (0, 1, 2), (0, 2, 3), ...;
        // fewer than three surviving vertices naturally produce no triangles.
        for (size_t triangle = 1; triangle + 1 < count; ++triangle) {
            TransformedVertex const clipped[] = {polygon[0], polygon[triangle], polygon[triangle + 1]};
            ScreenTriangle output{};

            /************************************ Todo Begin *************************************/

            for (int corner = 0; corner < 3; ++corner) {
                // Perspective divide maps clip space to NDC in [-1, 1]. Keep 1/w
                // for perspective-correct interpolation in the pixel loop.
                float const reciprocalW = 1.0f / clipped[corner].clip.w;
                glm::vec3 const ndc = glm::vec3(clipped[corner].clip) * reciprocalW;
                // Map NDC x/y to framebuffer pixels and z to depth in [0, 1].
                output.vertices[corner].screenAndInvW = glm::vec4((ndc.x * 0.5f + 0.5f) * extent.x,
                                                                  (ndc.y * 0.5f + 0.5f) * extent.y,
                                                                  ndc.z * 0.5f + 0.5f,
                                                                  reciprocalW);
                output.vertices[corner].worldPosition = clipped[corner].worldPosition;
                output.vertices[corner].normal = clipped[corner].normal;
                output.vertices[corner].uv = clipped[corner].uv;
            }

            // World-space edges and UV differences define the tangent-space axes.
            // A zero UV determinant means this triangle cannot define that basis;
            // in that case, keep ScreenTriangle's default axes.
            glm::vec3 const edge1 = clipped[1].worldPosition - clipped[0].worldPosition;
            glm::vec3 const edge2 = clipped[2].worldPosition - clipped[0].worldPosition;
            glm::vec2 const deltaUv1 = clipped[1].uv - clipped[0].uv;
            glm::vec2 const deltaUv2 = clipped[2].uv - clipped[0].uv;
            float const determinant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
            if (std::abs(determinant) > 1e-8f) {
                output.tangent = glm::normalize((edge1 * deltaUv2.y - edge2 * deltaUv1.y) / determinant);
                output.bitangent = glm::normalize((-edge1 * deltaUv2.x + edge2 * deltaUv1.x) / determinant);
            }
            // A positive 2D cross product is front-facing in our screen coordinates.
            // Reject back-facing and zero-area triangles before scanning pixels.
            glm::vec2 const a(output.vertices[0].screenAndInvW);
            glm::vec2 const b(output.vertices[1].screenAndInvW);
            glm::vec2 const c(output.vertices[2].screenAndInvW);
            if (glm::cross(b - a, c - a) > 0.0f) {
                result.push_back(output);
            }

            /************************************* Todo End **************************************/
        }
    }

    return result;
}

static void rasterizeTriangles(std::vector<ScreenTriangle> const& triangles,
                               PbrMaterial const& material,
                               FrameState const& frame,
                               glm::uvec2 extent,
                               std::vector<glm::vec4>& colorBuffer,
                               std::vector<float>& depthBuffer) {
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
        int const maxX = std::min(static_cast<int>(extent.x) - 1, static_cast<int>(std::ceil(maxPoint.x)));
        int const maxY = std::min(static_cast<int>(extent.y) - 1, static_cast<int>(std::ceil(maxPoint.y)));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                size_t const pixelIndex = static_cast<size_t>(y) * extent.x + x;

                glm::vec2 const p(float(x) + 0.5f, float(y) + 0.5f);
                // TODO: Calculate barycentric coordinates of the pixel
                // Hint: use the screen triangle vertices a, b, c
                glm::vec3 barycentric(glm::cross(c - b, p - b), glm::cross(a - c, p - c), glm::cross(b - a, p - a));
                if (barycentric.x < 0.0f || barycentric.y < 0.0f || barycentric.z < 0.0f) {
                    continue;
                }

                barycentric /= area;

                // TODO: Perform depth test
                // Hint: Compare the calculated pixel depth with depthBuffer[pixelIndex]
                // Hint: depthBuffer[pixelIndex] = ...

                // Cross products test pixel-center coverage and produce barycentric coordinates.
                float const depth = glm::dot(barycentric,
                                             glm::vec3(triangle.vertices[0].screenAndInvW.z,
                                                       triangle.vertices[1].screenAndInvW.z,
                                                       triangle.vertices[2].screenAndInvW.z));
                if (depth >= depthBuffer[pixelIndex]) {
                    continue;
                }

                depthBuffer[pixelIndex] = depth;

                // TODO: Shade the pixel with pbrShade
                // Hint: Calculate the following attributes of the pixel with perspective correction
                //       + position
                //       + normal
                //       + uv
                // Hint: colorBuffer[pixel] = ...;

                // Reweight by 1/w and normalize to interpolate attributes with perspective
                // correction.
                glm::vec3 const corrected = barycentric * glm::vec3(triangle.vertices[0].screenAndInvW.w,
                                                                    triangle.vertices[1].screenAndInvW.w,
                                                                    triangle.vertices[2].screenAndInvW.w);
                glm::vec3 const weights = corrected / (corrected.x + corrected.y + corrected.z);
                glm::vec3 position(0.0f);
                glm::vec3 normal(0.0f);
                glm::vec2 uv(0.0f);
                for (int corner = 0; corner < 3; ++corner) {
                    position += weights[corner] * triangle.vertices[corner].worldPosition;
                    normal += weights[corner] * triangle.vertices[corner].normal;
                    uv += weights[corner] * triangle.vertices[corner].uv;
                }

                // Shade the visible sample with material textures and direct-light PBR.
                colorBuffer[pixelIndex] = glm::vec4(
                    pbrShade(material, frame, position, normal, triangle.tangent, triangle.bitangent, uv), 1.0f);
            }
        }
    }
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

void CpuRenderer::clear() {
    std::fill(m_color.begin(), m_color.end(), m_backgroundColor);
    std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), 1.0f);
}

void CpuRenderer::render(DrawItem const& drawItem, FrameState const& frame) {
    // Transform and assemble this draw item's indexed triangles.
    auto const vertices = transformVertices(*drawItem.mesh, frame);
    auto const triangles = buildScreenTriangles(vertices, drawItem.mesh->indices(), frame.extent);

    // Rasterize and shade while preserving depth and color from previous draw items.
    rasterizeTriangles(triangles, *drawItem.material, frame, m_extent, m_color, m_depthBuffer);
}

gfx::Texture2D const& CpuRenderer::outputTexture() const {
    // The app calls this once after all draw items, including empty scenes.
    m_outputTexture->update(m_color.data(), gfx::Texture::ExternalFormat::RGBA, gfx::Texture::PixelType::Float);
    return Renderer::outputTexture();
}
