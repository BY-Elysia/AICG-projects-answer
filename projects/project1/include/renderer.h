#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "raster_types.h"

// Common render interface and HDR output for the CPU and GPU pipelines.
class Renderer {
public:
    Renderer(uint32_t width, uint32_t height);

    virtual ~Renderer() = default;

    void resize(glm::ivec2 const& extent);

    // Process all draw items together so depth and material ownership span the whole scene.
    virtual RendererStats render(std::vector<DrawItem> const& draws, FrameState const& frame) = 0;

    // SoftwareRendererApp presents the HDR result separately from the rendering pipeline.
    gfx::Texture2D const& outputTexture() const {
        return *m_outputTexture;
    }

protected:
    glm::ivec2 m_extent{0};
    // CPU uploads HDR pixels; GPU writes the same format through a framebuffer attachment.
    std::unique_ptr<gfx::Texture2D> m_outputTexture;

    virtual void resizeBackend() = 0;
};
