#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "graphics/rhi_resource.h"

namespace gfx {
/**
 * @brief   Independent sampler object
 * @details Although OpenGL has a default sampler combined with each texture object,
 *          we explicitly use a sampler object to sampler from the texture instead
 *          of using the default sampler, to be compatible with Vulkan.
 */
class Sampler : public RHIResource {
public:
    enum class Filter {
        Nearest,
        Linear,
    };

    enum class MipmapMode {
        None,
        Nearest,
        Linear,
    };

    enum class AddressMode {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
    };

    enum class CompareOp {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

public:
    struct Description {
        Filter minFilter{Filter::Nearest};
        Filter magFilter{Filter::Nearest};
        MipmapMode mipmapMode{MipmapMode::None};

        AddressMode addressModeU{AddressMode::Repeat};
        AddressMode addressModeV{AddressMode::Repeat};
        AddressMode addressModeW{AddressMode::Repeat};

        glm::vec4 borderColor{0.0f, 0.0f, 0.0f, 0.0f};

        bool compareEnable{false};
        CompareOp compareOp{CompareOp::Never};

        float mipLodBias{0.0f};
        float minLod{-1000.0f};
        float maxLod{+1000.0f};

        bool aniostropyEnable{false};
        float maxAnisotropy{1.0f};
    };

public:
    Sampler();

    Sampler(Description createInfo);

    Sampler(Sampler&& rhs) noexcept;

    ~Sampler();

    Sampler& operator=(Sampler&& rhs) noexcept;

    void bind(uint32_t texUnit) const;

    static void unbind(uint32_t texUnit);

    Filter getMinFilter(Filter filter) const noexcept {
        return m_description.minFilter;
    };

    Filter getMagFilter(Filter filter) const noexcept {
        return m_description.magFilter;
    };

    MipmapMode getMipmapMode(MipmapMode mipmap) const noexcept {
        return m_description.mipmapMode;
    };

    AddressMode getAddressModeU() const noexcept {
        return m_description.addressModeU;
    };

    AddressMode getAddressModeV() const noexcept {
        return m_description.addressModeV;
    };

    AddressMode getAddressModeW() const noexcept {
        return m_description.addressModeW;
    };

private:
    Description m_description;
};
} // namespace gfx
