#pragma once

#include "graphics/texture/texture.h"

namespace gfx {
class Texture2DMultisample : public Texture {
public:
    struct Specification {
        uint32_t width{0};
        uint32_t height{0};
        uint32_t samples{1};
        bool fixedSampleLocations{false};
        Texture::Format format{Texture::Format::None};
        bool isImmutable{false};
    };

public:
    Texture2DMultisample(Specification const& spec);

    Texture2DMultisample(Texture2DMultisample&& rhs) noexcept = default;

    ~Texture2DMultisample() override = default;

    Texture2DMultisample& operator=(Texture2DMultisample&& rhs) noexcept = default;

    void resize(uint32_t width, uint32_t height);

    uint32_t getWidth() const noexcept {
        return m_width;
    }

    uint32_t getHeight() const noexcept {
        return m_height;
    }

    uint32_t getSamples() const noexcept {
        return m_samples;
    }

    bool isImmutable() const noexcept {
        return m_immutable;
    }

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_samples{1};
    bool m_fixSampleLocations{false};
    bool m_immutable{false};
};
} // namespace gfx
