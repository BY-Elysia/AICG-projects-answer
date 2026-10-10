#pragma once

#include <cstdint>

#include "graphics/rhi_resource.h"

namespace gfx {
class Renderbuffer : public RHIResource {
public:
    enum class Format {
        R8,
        R8UI,
        R8I,
        R16UI,
        R16I,
        R32UI,
        R32I,
        RG8,
        RG8UI,
        RG8I,
        RG16UI,
        RG16I,
        RG32UI,
        RG32I,
        RGB8,
        RGBA8,
        SRGBA8,
        RGBA8UI,
        RGBA8I,
        RGBA16UI,
        RGBA16I,
        RGBA32I,
        RGBA32UI,
        RGB565,
        RGB5A1,
        RGBA4,
        RGB10A2,
        RGB10A2UI,
        // Depth & Stencil
        Depth16,
        Depth24,
        Depth32F,
        Stencil8,
        Depth24Stencil8,
        Depth32FStencil8,
    };

public:
    Renderbuffer(uint32_t width, uint32_t height, Format format, uint32_t samples = 1);

    Renderbuffer(Renderbuffer&& rhs) noexcept = default;

    Renderbuffer& operator=(Renderbuffer&& rhs) noexcept;

    ~Renderbuffer();

    Format getFormat() const noexcept {
        return m_format;
    }

    uint32_t getWidth() const noexcept {
        return m_width;
    }

    uint32_t getHeight() const noexcept {
        return m_height;
    }

    uint32_t getSamples() const noexcept {
        return m_samples;
    }

    void resize(uint32_t width, uint32_t height);

private:
    Format m_format;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_samples;
};
} // namespace gfx
