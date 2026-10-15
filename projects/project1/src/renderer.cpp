#include "renderer.h"

#include <limits>
#include <stdexcept>

static std::unique_ptr<gfx::Texture2D> makeRasterTexture(glm::uvec2 extent) {
    auto texture = std::make_unique<gfx::Texture2D>(gfx::Texture2D::Specification{
        .width = extent.x,
        .height = extent.y,
        .format = gfx::Texture::Format::RGBA16F,
        .pixelFormat = gfx::Texture::ExternalFormat::RGBA,
        .pixelType = gfx::Texture::PixelType::Float,
        .generateMipmap = false,
        .mipmapLevels = 1,
        .isImmutable = false,
    });
    return texture;
}

Renderer::Renderer(uint32_t width, uint32_t height) {
    m_extent = {width, height};
    m_outputTexture = makeRasterTexture(m_extent);
}

void Renderer::resize(glm::uvec2 const& extent) {
    if (extent == m_extent) {
        return;
    }

    m_extent = extent;
    m_outputTexture = makeRasterTexture(extent);
    resizeBackend();
}
