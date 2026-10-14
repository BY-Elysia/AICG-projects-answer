#pragma once

#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <graphics/texture/texture2d.h>

inline std::unique_ptr<gfx::Texture2D>
makeRasterTexture(glm::ivec2 extent,
                  gfx::Texture::Format format,
                  gfx::Texture::ExternalFormat pixelFormat,
                  gfx::Texture::PixelType pixelType = gfx::Texture::PixelType::Float,
                  void const* data = nullptr,
                  bool linearRepeat = false) {
    auto texture = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = static_cast<uint32_t>(extent.x),
            .height = static_cast<uint32_t>(extent.y),
            .format = format,
            .pixelFormat = pixelFormat,
            .pixelType = pixelType,
            .generateMipmap = false,
            .mipmapLevels = 1,
            .isImmutable = false
        },
        data);
    texture->bindTextureUnit(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearRepeat ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearRepeat ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, linearRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, linearRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    texture->unbindTextureUnit(0);
    return texture;
}
