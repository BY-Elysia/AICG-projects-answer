#pragma once

#include <cstdint>
#include <vector>

#include "renderer.h"

class CpuRenderer final : public Renderer {
public:
    CpuRenderer(uint32_t width, uint32_t height);

    void clear() override;

    void render(DrawItem const& drawItem, FrameState const& frame) override;

    gfx::Texture2D const& outputTexture() const override;

private:
    // Per-pixel linear HDR color before glTexSubImage2D upload.
    std::vector<glm::vec4> m_color;

    // D32F-equivalent software depth buffer. Smaller normalized depth is closer.
    std::vector<float> m_depthBuffer;

private:
    // Recreates the CPU-only visibility and color arrays after Renderer resizes its output image.
    void resizeBackend() override;
};
