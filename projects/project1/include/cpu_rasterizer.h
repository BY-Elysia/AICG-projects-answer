#pragma once

#include <vector>

#include "rasterizer.h"

class CpuRasterizer final : public Rasterizer {
public:
    CpuRasterizer() = default;

    RasterizerStats render(
        const std::vector<PreparedTriangle>& triangles, const PbrMaterial& material,
        const FrameState& frame) override;

private:
    // D32F-equivalent software depth buffer. Smaller normalized depth is closer.
    std::vector<float> m_depthBuffer;

    // Primitive ID of the triangle that last passed the depth test for each pixel.
    std::vector<uint32_t> m_ownerBuffer;

    // Per-pixel linear HDR color before glTexSubImage2D upload.
    std::vector<glm::vec4> m_color;

private:
    // Recreates the CPU-only visibility and color arrays after Rasterizer resizes its output image.
    void resizeBackend() override;

    // Returns the signed z component of the two-dimensional cross product lhs x rhs.
    static float cross2D(const glm::vec2& lhs, const glm::vec2& rhs) noexcept;
};
