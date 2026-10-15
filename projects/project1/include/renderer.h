#pragma once

#include <cstdint>
#include <memory>

#include "raster_types.h"

// Common render interface and HDR output for the CPU and GPU pipelines.
class Renderer {
public:
    Renderer(uint32_t width, uint32_t height);

    virtual ~Renderer() = default;

    void resize(glm::uvec2 const& extent);

    void setBackgroundColor(glm::vec4 const& color) {
        m_backgroundColor = color;
    }

    // Begin a frame by clearing its color and depth buffers.
    virtual void clear() = 0;

    // Draw one validated item into the buffers initialized by clear().
    virtual void render(DrawItem const& drawItem, FrameState const& frame) = 0;

    // SoftwareRendererApp presents the HDR result separately from the rendering pipeline.
    virtual gfx::Texture2D const& outputTexture() const {
        return *m_outputTexture;
    }

protected:
    glm::uvec2 m_extent{0};
    glm::vec4 m_backgroundColor{0.025f, 0.03f, 0.04f, 1.0f};
    // CPU uploads HDR pixels; GPU writes the same format through a framebuffer attachment.
    std::unique_ptr<gfx::Texture2D> m_outputTexture;

    virtual void resizeBackend() = 0;
};
