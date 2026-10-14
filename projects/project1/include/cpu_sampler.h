#pragma once

#include <glm/glm.hpp>

class CpuTexture2D;

class CpuSampler {
public:
    enum class Filter { Nearest, Linear };

    enum class MipmapMode { None, Nearest, Linear };

    enum class AddressMode { Repeat, MirroredRepeat, ClampToEdge };

    struct Description {
        Filter minFilter{Filter::Nearest};
        Filter magFilter{Filter::Nearest};
        MipmapMode mipmapMode{MipmapMode::None};
        AddressMode addressModeU{AddressMode::Repeat};
        AddressMode addressModeV{AddressMode::Repeat};
    };

public:
    CpuSampler() = default;

    explicit CpuSampler(Description description)
        : m_description(description) {}

    // Without texture-coordinate derivatives, sample() uses the base level and minification filter.
    glm::vec4 sample(CpuTexture2D const& texture, glm::vec2 const& uv) const;

    // Samples the mip chain at an explicit LOD; negative LOD uses the magnification filter.
    glm::vec4 sampleMipMap(CpuTexture2D const& texture, glm::vec2 const& uv, float levelOfDetail) const;

private:
    Description m_description;

    glm::vec4 sampleLevel(CpuTexture2D const& texture, glm::vec2 const& uv, size_t level, Filter filter) const;

    static int resolveCoordinate(int coordinate, int extent, AddressMode addressMode) noexcept;

    static float resolveUv(float coordinate, AddressMode addressMode) noexcept;
};
