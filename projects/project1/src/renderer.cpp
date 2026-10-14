#include "renderer.h"
#include "texture_utils.h"

#include <limits>
#include <stdexcept>

Renderer::Renderer(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Render extent must be positive and fit in int");
    }
    m_extent = {static_cast<int>(width), static_cast<int>(height)};
    m_outputTexture = makeRasterTexture(m_extent, gfx::Texture::Format::RGBA16F, gfx::Texture::ExternalFormat::RGBA);
}

void Renderer::resize(glm::ivec2 const& extent) {
    if (extent.x <= 0 || extent.y <= 0) {
        throw std::invalid_argument("Render extent must be positive");
    }
    if (extent == m_extent) {
        return;
    }
    m_extent = extent;
    m_outputTexture = makeRasterTexture(extent, gfx::Texture::Format::RGBA16F, gfx::Texture::ExternalFormat::RGBA);
    resizeBackend();
}
