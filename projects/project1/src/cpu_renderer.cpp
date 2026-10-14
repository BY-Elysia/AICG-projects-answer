#define GLM_ENABLE_EXPERIMENTAL
#include "cpu_renderer.h"

#include <limits>
#include <stdexcept>
#include <common/timer.h>
#include <glm/gtx/exterior_product.hpp>

#include "pbr_shading.h"

void CpuRenderer::resizeBackend() {
    // Keep separate depth and primitive-owner buffers, mirroring the GPU visibility resources.
    size_t const pixels = static_cast<size_t>(m_extent.x) * m_extent.y;
    m_depthBuffer.resize(pixels);
    m_ownerBuffer.resize(pixels);
    m_color.resize(pixels);
}

RendererStats CpuRenderer::render(std::span<DrawItem const> draws, FrameState const& frame) {
    resize(frame.extent);
    Timer geometryTimer;
    geometryTimer.tick();
    std::vector<PreparedTriangle> triangles;
    std::vector<PbrMaterial const*> triangleMaterials;
    for (DrawItem const& draw : draws) {
        if (!draw.mesh || !draw.material) {
            throw std::invalid_argument("Draw item needs a mesh and material");
        }
        auto prepared = prepareTriangles(*draw.mesh, draw.transform, frame);
        if (prepared.size() > std::numeric_limits<uint32_t>::max() - triangles.size()) {
            throw std::runtime_error("Too many rasterizer triangles");
        }
        triangleMaterials.insert(triangleMaterials.end(), prepared.size(), draw.material);
        triangles.insert(triangles.end(), prepared.begin(), prepared.end());
    }
    float const preparationMs = static_cast<float>(geometryTimer.tock() * 1000.0);

    Timer timer;
    timer.tick();
    // Clear depth to the far plane, clear owners to "no primitive", then paint the background.
    std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), 1.0f);
    std::fill(m_ownerBuffer.begin(), m_ownerBuffer.end(), std::numeric_limits<uint32_t>::max());
    std::fill(m_color.begin(), m_color.end(), glm::vec4(0.025f, 0.03f, 0.04f, 1.0f));
    for (uint32_t triangleIndex = 0; triangleIndex < triangles.size(); ++triangleIndex) {
        PreparedTriangle const& triangle = triangles[triangleIndex];
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
                m_ownerBuffer[pixelIndex] = triangleIndex;
                // Reweight by 1/w and normalize to interpolate attributes with perspective
                // correction.
                glm::vec3 const corrected = barycentric * glm::vec3(triangle.vertices[0].screenAndInvW.w,
                                                                    triangle.vertices[1].screenAndInvW.w,
                                                                    triangle.vertices[2].screenAndInvW.w);
                glm::vec3 const weights = corrected / (corrected.x + corrected.y + corrected.z);
                glm::vec3 position(0.0f), normal(0.0f);
                glm::vec2 uv(0.0f);
                for (int corner = 0; corner < 3; ++corner) {
                    position += weights[corner] * glm::vec3(triangle.vertices[corner].worldPosition);
                    normal += weights[corner] * glm::vec3(triangle.vertices[corner].normal);
                    uv += weights[corner] * glm::vec2(triangle.vertices[corner].uv);
                }
                // Shade the visible sample with material textures and direct-light PBR.
                m_color[pixelIndex] = glm::vec4(pbr::shade(*triangleMaterials[triangleIndex],
                                                           frame,
                                                           position,
                                                           normal,
                                                           glm::vec3(triangle.tangent),
                                                           glm::vec3(triangle.bitangent),
                                                           uv),
                                                1.0f);
            }
        }
    }
    // Upload the CPU result so the same full-screen pass can present either renderer.
    m_outputTexture->update(m_color.data(), gfx::Texture::ExternalFormat::RGBA, gfx::Texture::PixelType::Float);
    return {static_cast<float>(timer.tock() * 1000.0), static_cast<uint32_t>(triangles.size()), 0.0f, preparationMs};
}
