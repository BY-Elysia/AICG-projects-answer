#pragma once

#include "graphics/texture/texture.h"

namespace gfx {
class Texture2D : public Texture {
public:
    struct Specification {
        uint32_t width{0};
        uint32_t height{0};
        Texture::Format format{Texture::Format::None};
        Texture::ExternalFormat pixelFormat{Texture::ExternalFormat::None};
        Texture::PixelType pixelType{Texture::PixelType::None};
        bool generateMipmap{false};
        uint32_t mipmapLevels{0};
        bool isImmutable{false};
    };

public:
    Texture2D(Specification const& spec, void const* data = nullptr);

    Texture2D(Texture2D&& rhs) noexcept = default;

    ~Texture2D() override = default;

    Texture2D& operator=(Texture2D&& rhs) noexcept = default;

    uint32_t getNativeType() const override;

    uint32_t getWidth() const noexcept {
        return m_width;
    }

    uint32_t getHeight() const noexcept {
        return m_height;
    }

    uint32_t getMipmapLevels() const noexcept {
        return m_mipmapLevels;
    }

    void generateMipmap();

    void resize(uint32_t width, uint32_t height);

    void update(void const* data, ExternalFormat pixelFormat, PixelType type, uint32_t level = 0);

    void bindImage(uint32_t unit, Access access, int level = 0);

    void unbindImage(uint32_t unit);

    void copyFromFramebuffer(int x, int y, int width, int height, int level = 0);

    bool isImmutable() const noexcept {
        return m_immutable;
    }

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_mipmapLevels{1};
    bool m_immutable{false};
};
} // namespace gfx