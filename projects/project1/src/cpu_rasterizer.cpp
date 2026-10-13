#define GLM_ENABLE_EXPERIMENTAL
#include "cpu_rasterizer.h"

#include <limits>
#include <common/timer.h>
#include <glm/gtx/exterior_product.hpp>

#include "pbr_shading.h"

void CpuRasterizer::resizeBackend() {
    // Keep separate depth and primitive-owner buffers, mirroring the GPU visibility resources.
    const size_t pixels = static_cast<size_t>(m_extent.x) * m_extent.y;
    m_depthBuffer.resize(pixels);
    m_ownerBuffer.resize(pixels);
    m_color.resize(pixels);
}

RasterizerStats CpuRasterizer::render(
    const std::vector<PreparedTriangle>& triangles, const PbrMaterial& material,
    const FrameState& frame) {
    Timer timer;
    timer.tick();
    // Clear depth to the far plane, clear owners to "no primitive", then paint the background.
    std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), 1.0f);
    std::fill(m_ownerBuffer.begin(), m_ownerBuffer.end(), std::numeric_limits<uint32_t>::max());
    std::fill(m_color.begin(), m_color.end(), glm::vec4(0.025f, 0.03f, 0.04f, 1.0f));
    for (uint32_t triangleIndex = 0; triangleIndex < triangles.size(); ++triangleIndex) {
        const PreparedTriangle& triangle = triangles[triangleIndex];
        const glm::vec2 a(triangle.vertices[0].screenAndInvW);
        const glm::vec2 b(triangle.vertices[1].screenAndInvW);
        const glm::vec2 c(triangle.vertices[2].screenAndInvW);
        const float area = glm::cross(b - a, c - a);
        if (area <= 0.0f) {
            continue;
        }
        // Restrict coverage tests to the triangle's screen-space bounding box.
        const glm::vec2 minPoint = glm::min(a, glm::min(b, c));
        const glm::vec2 maxPoint = glm::max(a, glm::max(b, c));
        const int minX = std::max(0, static_cast<int>(std::floor(minPoint.x)));
        const int minY = std::max(0, static_cast<int>(std::floor(minPoint.y)));
        const int maxX = std::min(m_extent.x - 1, static_cast<int>(std::ceil(maxPoint.x)));
        const int maxY = std::min(m_extent.y - 1, static_cast<int>(std::ceil(maxPoint.y)));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const glm::vec2 sample(float(x) + 0.5f, float(y) + 0.5f);
                // Cross products test pixel-center coverage and produce barycentric coordinates.
                glm::vec3 barycentric(
                    glm::cross(c - b, sample - b), glm::cross(a - c, sample - c),
                    glm::cross(b - a, sample - a));
                if (barycentric.x < 0.0f || barycentric.y < 0.0f || barycentric.z < 0.0f) {
                    continue;
                }
                barycentric /= area;
                const float depth = glm::dot(
                    barycentric,
                    glm::vec3(
                        triangle.vertices[0].screenAndInvW.z, triangle.vertices[1].screenAndInvW.z,
                        triangle.vertices[2].screenAndInvW.z));
                const size_t pixelIndex = static_cast<size_t>(y) * m_extent.x + x;
                if (depth >= m_depthBuffer[pixelIndex]) {
                    continue;
                }
                m_depthBuffer[pixelIndex] = depth;
                m_ownerBuffer[pixelIndex] = triangleIndex;
                // Reweight by 1/w and normalize to interpolate attributes with perspective
                // correction.
                const glm::vec3 corrected =
                    barycentric
                    * glm::vec3(
                        triangle.vertices[0].screenAndInvW.w, triangle.vertices[1].screenAndInvW.w,
                        triangle.vertices[2].screenAndInvW.w);
                const glm::vec3 weights = corrected / (corrected.x + corrected.y + corrected.z);
                glm::vec3 position(0.0f), normal(0.0f);
                glm::vec2 uv(0.0f);
                for (int corner = 0; corner < 3; ++corner) {
                    position +=
                        weights[corner] * glm::vec3(triangle.vertices[corner].worldPosition);
                    normal += weights[corner] * glm::vec3(triangle.vertices[corner].normal);
                    uv += weights[corner] * glm::vec2(triangle.vertices[corner].uv);
                }
                // Shade the visible sample with material textures and direct-light PBR.
                m_color[pixelIndex] = glm::vec4(
                    pbr::shade(
                        material, frame, position, normal, glm::vec3(triangle.tangent),
                        glm::vec3(triangle.bitangent), uv),
                    1.0f);
            }
        }
    }
    // Upload the CPU result so the same full-screen pass can present either renderer.
    m_outputTexture->update(m_color.data(), gfx::Texture::ExternalFormat::RGBA,
                            gfx::Texture::PixelType::Float);
    return {
        static_cast<float>(timer.tock() * 1000.0),
        static_cast<uint32_t>(triangles.size())};
}
