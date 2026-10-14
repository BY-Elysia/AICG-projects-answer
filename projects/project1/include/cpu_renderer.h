#pragma once

#include <vector>

#include "renderer.h"

class CpuRenderer final : public Renderer {
public:
    CpuRenderer() = default;

    RendererStats render(std::span<DrawItem const> draws, FrameState const& frame) override;

private:
    // Per-pixel linear HDR color before glTexSubImage2D upload.
    std::vector<glm::vec4> m_color;

    // D32F-equivalent software depth buffer. Smaller normalized depth is closer.
    std::vector<float> m_depthBuffer;

    // Primitive ID of the triangle that last passed the depth test for each pixel.
    std::vector<uint32_t> m_ownerBuffer;

private:
    // Recreates the CPU-only visibility and color arrays after Renderer resizes its output image.
    void resizeBackend() override;
};
