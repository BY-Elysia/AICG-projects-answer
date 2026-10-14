#pragma once

#include <vector>

#include "renderer.h"

class CpuRenderer final : public Renderer {
public:
    CpuRenderer() = default;

protected:
    RendererStats renderPrepared(std::vector<PreparedTriangle> const& triangles,
                                 PbrMaterial const& material,
                                 FrameState const& frame) override;

private:
    // D32F-equivalent software depth buffer. Smaller normalized depth is closer.
    std::vector<float> m_depthBuffer;

    // Primitive ID of the triangle that last passed the depth test for each pixel.
    std::vector<uint32_t> m_ownerBuffer;

    // Per-pixel linear HDR color before glTexSubImage2D upload.
    std::vector<glm::vec4> m_color;

private:
    // Recreates the CPU-only visibility and color arrays after Renderer resizes its output image.
    void resizeBackend() override;
};
