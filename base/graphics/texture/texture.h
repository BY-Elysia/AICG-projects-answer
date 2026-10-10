#pragma once

#include <cstdint>

#include "graphics/rhi_resource.h"
#include "graphics/sampler.h"

namespace gfx {
class Texture : public RHIResource {
public:
    // pixel format in GPU memory
    enum class Format {
        None,
        R8,
        R8SN,
        R16,
        R16SN,
        RG8,
        RG8SN,
        RG16,
        RG16SN,
        RGB8,
        RGB8SN,
        RGB16,
        RGB16SN,
        RGBA8,
        RGBA8SN,
        RGBA16,
        RGBA16SN,
        R16F,
        RG16F,
        RGB16F,
        RGBA16F,
        R32F,
        RG32F,
        RGB32F,
        RGBA32F,
        R8I,
        R8UI,
        R16I,
        R16UI,
        R32I,
        R32UI,
        RG8I,
        RG8UI,
        RG16I,
        RG16UI,
        RG32I,
        RG32UI,
        RGB8I,
        RGB8UI,
        RGB16I,
        RGB16UI,
        RGB32I,
        RGB32UI,
        RGBA8I,
        RGBA8UI,
        RGBA16I,
        RGBA16UI,
        RGBA32I,
        RGBA32UI,
        // sRGB
        SRGB8,
        SRGBA8,
        // wired internal format
        R3G3B2,
        RGB4,
        RGB5,
        RGB10,
        RGB12,
        RGBA2,
        RGBA4,
        RGB5A1,
        RGB10A2,
        RGB10A2UI,
        RGBA12,
        R11G11B10F,
        RGB9E5,
        // depth & stencil
        Depth16,
        Depth24,
        Depth32,
        Depth32F,
        Stencil8,
        Depth24Stencil8,
        Depth32FStencil8,

        // compressed internal format
        // 1. generic compressed internal format
        CompressRed,
        CompressRG,
        CompressRGB,
        CompressRGBA,
        CompressSRGB,
        CompressSRGBA,
        // 2. specified compressed internal format
        CompressRed_RGTC1,
        CompressSRed_RGTC1,
        CompressRG_RGTC2,
        CompressSRG_RGTC2,
        CompressRGBA_BPTC_UN,
        CompressSRGBA_BPTC_UN,
        CompressRGB_BPTC_SF,
        CompressRGB_BPTC_UF,
    };

    // @ref https://www.khronos.org/opengl/wiki/Pixel_Transfer
    // transfer pixel format
    enum class ExternalFormat {
        None,
        // normalized integer / floating point data
        Red,
        RG,
        RGB,
        BGR,
        RGBA,
        BGRA,
        // non-normalized integer data
        RedInt,
        RGInt,
        RGBInt,
        BGRInt,
        RGBAInt,
        BGRAInt,
        // depth & stencil
        Depth,
        Stencil,
        DepthStencil
    };

    // transfer pixel type: each component / packed multiple components
    enum class PixelType {
        None,
        UByte,
        Byte,
        UShort,
        Short,
        UInt,
        Int,
        HalfFloat,
        Float,
        // We don't support them yet
        // packed type, REV denotes lsb to msb
        UByte3_3_2,
        UByte2_3_3_REV,
        UShort5_6_5,
        UShort5_6_5_REV,
        UShort4_4_4_4,
        UShort4_4_4_4_REV,
        UShort5_5_5_1,
        UShort1_5_5_5_REV,
        UInt8_8_8_8,
        UInt8_8_8_8_REV,
        UInt10_10_10_2,
        UInt2_10_10_10_REV,
        UInt24_8,
        UInt5_9_9_9_REV,
        B10G11R11F_REV,
        Float32_UInt24_8_REV, // GL_DEPTH32F_STENCIL8
    };

    enum class Access {
        ReadOnly,
        WriteOnly,
        ReadWrite,
    };

public:
    Texture(Format format);

    Texture(Texture&& rhs) noexcept;

    virtual ~Texture();

    Texture& operator=(Texture&& rhs) noexcept;

    Format getFormat() const noexcept {
        return m_format;
    };

    void bindTextureUnit(uint32_t texUnit) const;

    static void unbindTextureUnit(uint32_t texUnit);

    void clear(ExternalFormat format, PixelType type, void const* data, int level = 0);

    virtual uint32_t getNativeType() const = 0;

    /* APIs for bindless texture */
    // Once the texture is made as bindless texture by `makeDeviceResident`,
    // + the texture memory should not be reallocated, but can be updated
    // + the sample info will be baked into the texture object, and become immutable
    void makeDeviceResident(Sampler const& sampler);

    void makeDeviceNonResident();

    uint64_t getDevicePtr() const noexcept;

protected:
    Format m_format{Format::None};
    uint64_t m_devicePtr{0};
};
} // namespace gfx
