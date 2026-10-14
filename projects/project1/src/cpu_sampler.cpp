#include "cpu_sampler.h"

#include <algorithm>
#include <cmath>

#include "cpu_texture2d.h"

glm::vec4 CpuSampler::sample(CpuTexture2D const& texture, glm::vec2 const& uv) const {
    return sampleLevel(texture, uv, 0, m_description.minFilter);
}

glm::vec4 CpuSampler::sampleMipMap(CpuTexture2D const& texture, glm::vec2 const& uv, float levelOfDetail) const {
    if (levelOfDetail < 0.0f) {
        return sampleLevel(texture, uv, 0, m_description.magFilter);
    }

    if (m_description.mipmapMode == MipmapMode::None) {
        return sample(texture, uv);
    }

    float const maxLevel = static_cast<float>(texture.m_mipLevels.size() - 1);
    float const level = std::clamp(levelOfDetail, 0.0f, maxLevel);
    if (m_description.mipmapMode == MipmapMode::Nearest) {
        return sampleLevel(texture, uv, static_cast<size_t>(std::floor(level + 0.5f)), m_description.minFilter);
    }

    size_t const lowerLevel = static_cast<size_t>(std::floor(level));
    size_t const upperLevel = std::min(lowerLevel + 1, texture.m_mipLevels.size() - 1);
    float const blend = level - static_cast<float>(lowerLevel);
    glm::vec4 const lower = sampleLevel(texture, uv, lowerLevel, m_description.minFilter);
    glm::vec4 const upper = sampleLevel(texture, uv, upperLevel, m_description.minFilter);

    return glm::mix(lower, upper, blend);
}

glm::vec4
CpuSampler::sampleLevel(CpuTexture2D const& texture, glm::vec2 const& inputUv, size_t level, Filter filter) const {
    CpuTexture2D::MipLevel const& mipLevel = texture.m_mipLevels[std::min(level, texture.m_mipLevels.size() - 1)];
    int const width = mipLevel.width;
    int const height = mipLevel.height;
    glm::vec2 const uv(resolveUv(inputUv.x, m_description.addressModeU),
                       resolveUv(inputUv.y, m_description.addressModeV));
    auto texel = [&](int x, int y) {
        return texture.texelFromLevel(mipLevel.pixels,
                                      width,
                                      height,
                                      resolveCoordinate(x, width, m_description.addressModeU),
                                      resolveCoordinate(y, height, m_description.addressModeV));
    };

    if (filter == Filter::Nearest) {
        return texel(static_cast<int>(std::floor(uv.x * static_cast<float>(width))),
                     static_cast<int>(std::floor(uv.y * static_cast<float>(height))));
    }

    float const x = uv.x * static_cast<float>(width) - 0.5f;
    float const y = uv.y * static_cast<float>(height) - 0.5f;
    int const x0 = static_cast<int>(std::floor(x));
    int const y0 = static_cast<int>(std::floor(y));
    float const tx = x - std::floor(x);
    float const ty = y - std::floor(y);
    glm::vec4 const lower = glm::mix(texel(x0, y0), texel(x0 + 1, y0), tx);
    glm::vec4 const upper = glm::mix(texel(x0, y0 + 1), texel(x0 + 1, y0 + 1), tx);

    return glm::mix(lower, upper, ty);
}

int CpuSampler::resolveCoordinate(int coordinate, int extent, AddressMode addressMode) noexcept {
    switch (addressMode) {
    case AddressMode::Repeat:
        return (coordinate % extent + extent) % extent;
    case AddressMode::MirroredRepeat: {
        int const mirroredExtent = extent * 2;
        int const wrapped = (coordinate % mirroredExtent + mirroredExtent) % mirroredExtent;
        return wrapped < extent ? wrapped : mirroredExtent - 1 - wrapped;
    }
    case AddressMode::ClampToEdge:
        return std::clamp(coordinate, 0, extent - 1);
    }
    return std::clamp(coordinate, 0, extent - 1);
}

float CpuSampler::resolveUv(float coordinate, AddressMode addressMode) noexcept {
    switch (addressMode) {
    case AddressMode::Repeat:
        return glm::fract(coordinate);
    case AddressMode::MirroredRepeat: {
        float const interval = std::floor(coordinate);
        float const fraction = coordinate - interval;
        return std::fmod(interval, 2.0f) == 0.0f ? fraction : 1.0f - fraction;
    }
    case AddressMode::ClampToEdge:
        return std::clamp(coordinate, 0.0f, 1.0f);
    }

    return std::clamp(coordinate, 0.0f, 1.0f);
}
