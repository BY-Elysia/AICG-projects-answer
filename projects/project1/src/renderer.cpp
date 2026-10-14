#include "renderer.h"

#include <limits>
#include <stdexcept>

static std::unique_ptr<gfx::Texture2D> makeRasterTexture(glm::ivec2 extent) {
    auto texture = std::make_unique<gfx::Texture2D>(gfx::Texture2D::Specification{
        .width = static_cast<uint32_t>(extent.x),
        .height = static_cast<uint32_t>(extent.y),
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
    if (width == 0 || height == 0 || width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Render extent must be positive and fit in int");
    }
    m_extent = {static_cast<int>(width), static_cast<int>(height)};
    m_outputTexture = makeRasterTexture(m_extent);
}

void Renderer::resize(glm::ivec2 const& extent) {
    if (extent.x <= 0 || extent.y <= 0) {
        throw std::invalid_argument("Render extent must be positive");
    }
    if (extent == m_extent) {
        return;
    }
    m_extent = extent;
    m_outputTexture = makeRasterTexture(extent);
    resizeBackend();
}
