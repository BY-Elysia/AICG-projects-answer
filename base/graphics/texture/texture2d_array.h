#pragma once

#include "graphics/texture/texture.h"

namespace gfx {
class Texture2DArray : public Texture {
public:
    struct Specification {
        uint32_t width{0};
        uint32_t height{0};
        uint32_t layers{0};
        Texture::Format format{Texture::Format::None};
        bool generateMipmap{false};
        uint32_t mipmapLevels{0};
        bool isImmutable{false};
    };

public:
    Texture2DArray(Specification const& spec);

    Texture2DArray(Texture2DArray&& rhs) noexcept = default;

    ~Texture2DArray() override = default;

    Texture2DArray& operator=(Texture2DArray&& rhs) noexcept = default;

    uint32_t getNativeType() const override;

    void generateMipmap();

    void update(void const* data, ExternalFormat pixelFormat, PixelType type, uint32_t layer, uint32_t mipmaplevel = 0);

    uint32_t getWidth() const noexcept {
        return m_width;
    }

    uint32_t getHeight() const noexcept {
        return m_height;
    }

    uint32_t getLayers() const noexcept {
        return m_layers;
    }

    uint32_t getMipmapLevels() const noexcept {
        return m_mipmapLevels;
    }

    bool isImmutable() const noexcept {
        return m_immutable;
    };

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_layers{0};
    uint32_t m_mipmapLevels{1};
    bool m_immutable{false};

private:
    static uint32_t getMaxMipmapLevels(uint32_t width, uint32_t height);
};
} // namespace gfx