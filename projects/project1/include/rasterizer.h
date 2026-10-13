#pragma once

#include <memory>
#include <vector>

#include "raster_types.h"
#include "texture_utils.h"
#include <stdexcept>

class Rasterizer {
public:
    virtual ~Rasterizer() = default;

    // Resizes the common HDR output image, then lets the selected backend recreate its own
    // framebuffer resources for the same pixel extent.
    void resize(const glm::ivec2& extent) {
        if (extent.x <= 0 || extent.y <= 0) {
            throw std::invalid_argument("Raster extent must be positive");
        }
        if (extent == m_extent) {
            return;
        }

        m_extent = extent;
        m_outputTexture =
            makeRasterTexture(extent, gfx::Texture::Format::RGBA16F, gfx::Texture::ExternalFormat::RGBA);
        resizeBackend();
    }

    // Returns the common HDR image consumed by RasterizerApp's presentation pass.
    const gfx::Texture2D& outputTexture() const {
        return *m_outputTexture;
    }

    virtual RasterizerStats render(
        const std::vector<PreparedTriangle>& triangles, const PbrMaterial& material,
        const FrameState& frame) = 0;

protected:
    // Current raster target dimensions shared by CPU pixel loops and GPU fragment passes.
    glm::ivec2 m_extent{0};

    // Shared RGBA16F output image: CPU uploads pixels into it and GPU writes it as a framebuffer attachment.
    std::unique_ptr<gfx::Texture2D> m_outputTexture;

protected:
    // Recreates storage that is specific to the concrete rasterization implementation.
    virtual void resizeBackend() = 0;
};
