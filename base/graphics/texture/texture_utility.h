#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "graphics/gl_utility.h"
#include "graphics/texture/texture.h"

namespace gfx {
namespace details {
static GLenum toNativeExternalFormat(Texture::ExternalFormat format) {
    /*
     * @ref https://www.khronos.org/opengl/wiki/GLAPI/glTexImage2D
        Specifies the format of the pixel data.
        For transfers of depth, stencil, or depth/stencil data, you must use
            GL_DEPTH_COMPONENT,
            GL_STENCIL_INDEX, or
            GL_DEPTH_STENCIL
        For transfers of normalized integer or floating-point color image data, you must use
            GL_RED,
            GL_RG,
            GL_RGB,
            GL_BGR,
            GL_RGBA, or
            GL_BGRA.
        For transfers of non-normalized integer data, you must use:
            GL_RED_INTEGER,
            GL_RG_INTEGER,
            GL_RGB_INTEGER,
            GL_BGR_INTEGER,
            GL_RGBA_INTEGER, or
            GL_BGRA_INTEGER.
    */

    switch (format) {
    default:
        break;
    case Texture::ExternalFormat::Red:
        return GL_RED;
    case Texture::ExternalFormat::RG:
        return GL_RG;
    case Texture::ExternalFormat::RGB:
        return GL_RGB;
    case Texture::ExternalFormat::BGR:
        return GL_BGR;
    case Texture::ExternalFormat::RGBA:
        return GL_RGBA;
    case Texture::ExternalFormat::BGRA:
        return GL_BGRA;
    case Texture::ExternalFormat::RedInt:
        return GL_RED_INTEGER;
    case Texture::ExternalFormat::RGInt:
        return GL_RG_INTEGER;
    case Texture::ExternalFormat::RGBInt:
        return GL_RGB_INTEGER;
    case Texture::ExternalFormat::BGRInt:
        return GL_BGR_INTEGER;
    case Texture::ExternalFormat::RGBAInt:
        return GL_RGBA_INTEGER;
    case Texture::ExternalFormat::BGRAInt:
        return GL_RGBA_INTEGER;
    case Texture::ExternalFormat::Depth:
        return GL_DEPTH_COMPONENT;
    case Texture::ExternalFormat::Stencil:
        return GL_STENCIL_INDEX;
    case Texture::ExternalFormat::DepthStencil:
        return GL_DEPTH_STENCIL;
    }

    throw std::runtime_error("Invalid external texture format");
}

static GLint toNativeFormat(Texture::Format format) {
    /*
     * @ref https://www.khronos.org/opengl/wiki/GLAPI/glTexImage2D
     *      https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf
     *      https://stackoverflow.com/questions/65461779
    Specifies the number of color components in the texture.
    + Base interal format
        GL_DEPTH_COMPONENT
        GL_DEPTH_STENCIL
        GL_STENCIL_INDEX
        GL_RED
        GL_RG
        GL_RGB
        GL_RGBA
    + Sized internal format
                                                               Component Bitdepth
        Sized Internal Format    Base Internal Format   Red    Green      Blue     Alpha    Shared
        GL_R8                            GL_RED           8
        GL_R8_SNORM                      GL_RED          s8
        GL_R16                           GL_RED          16
        GL_R16_SNORM                     GL_RED         s16
        GL_RG8                           GL_RG            8        8
        GL_RG8_SNORM                     GL_RG           s8       s8
        GL_RG16                          GL_RG           16       16
        GL_RG16_SNORM                    GL_RG          s16      s16
        GL_RGB8                          GL_RGB           8        8        8
        GL_RGB8_SNORM                    GL_RGB          s8       s8       s8
        GL_RGB16                         GL_RGB          16       16       16
        GL_RGB16_SNORM                   GL_RGB         s16      s16      s16
        GL_RGBA8                         GL_RGBA          8        8        8        8
        GL_RGBA8_SNORM                   GL_RGBA         s8       s8       s8       s8
        GL_RGBA16                        GL_RGBA         16       16       16       16
        GL_R16F                          GL_RED         f16
        GL_RG16F                         GL_RG          f16      f16
        GL_RGB16F                        GL_RGB         f16      f16      f16
        GL_RGBA16F                       GL_RGBA        f16      f16      f16      f16
        GL_R32F                          GL_RED         f32
        GL_RG32F                         GL_RG          f32      f32
        GL_RGB32F                        GL_RGB         f32      f32      f32
        GL_RGBA32F                       GL_RGBA        f32      f32      f32      f32
        GL_R8I                           GL_RED          i8
        GL_R8UI                          GL_RED         ui8
        GL_R16I                          GL_RED         i16
        GL_R16UI                         GL_RED        ui16
        GL_R32I                          GL_RED         i32
        GL_R32UI                         GL_RED        ui32
        GL_RG8I                          GL_RG           i8       i8
        GL_RG8UI                         GL_RG          ui8      ui8
        GL_RG16I                         GL_RG          i16      i16
        GL_RG16UI                        GL_RG         ui16     ui16
        GL_RG32I                         GL_RG          i32      i32
        GL_RG32UI                        GL_RG         ui32     ui32
        GL_RGB8I                         GL_RGB          i8       i8       i8
        GL_RGB8UI                        GL_RGB         ui8      ui8      ui8
        GL_RGB16I                        GL_RGB         i16      i16      i16
        GL_RGB16UI                       GL_RGB        ui16     ui16     ui16
        GL_RGB32I                        GL_RGB         i32      i32      i32
        GL_RGB32UI                       GL_RGB        ui32     ui32     ui32
        GL_RGBA8I                        GL_RGBA         i8       i8       i8       i8
        GL_RGBA8UI                       GL_RGBA        ui8      ui8      ui8      ui8
        GL_RGBA16I                       GL_RGBA        i16      i16      i16      i16
        GL_RGBA16UI                      GL_RGBA       ui16     ui16     ui16     ui16
        GL_RGBA32I                       GL_RGBA        i32      i32      i32      i32
        GL_RGBA32UI                      GL_RGBA       ui32     ui32     ui32     ui32
        GL_SRGB8                         GL_RGB           8        8        8
        GL_SRGB8_ALPHA8                  GL_RGBA          8        8        8        8
        GL_R3_G3_B2                      GL_RGB           3        3        2
        GL_RGB4                          GL_RGB           4        4        4
        GL_RGB5                          GL_RGB           5        5        5
        GL_RGBA2                         GL_RGB           2        2        2        2
        GL_RGBA4                         GL_RGB           4        4        4        4
        GL_RGB5_A1                       GL_RGBA          5        5        5        1
        GL_RGB10_A2                      GL_RGBA         10       10       10        2
        GL_RGB10_A2UI                    GL_RGBA       ui10     ui10     ui10      ui2
        GL_RGBA12                        GL_RGBA         12       12       12       12
        GL_R11F_G11F_B10F                GL_RGB         f11      f11      f10
        GL_RGB9_E5                       GL_RGB           9        9        9        5       5
        GL_RGB10                         GL_RGB          10       10       10
        GL_RGB12                         GL_RGB          12       12       12
    + Sized Depth and Stencil Internal Formats
                                                               Component Bitdepth
        Sized Internal Format      Base Internal Format   Depth    Stencil
        GL_DEPTH_COMPONENT16       GL_DEPTH_COMPONENT       16
        GL_DEPTH_COMPONENT24       GL_DEPTH_COMPONENT       24
        GL_DEPTH_COMPONENT32       GL_DEPTH_COMPONENT       32
        GL_DEPTH_COMPONENT32F      GL_DEPTH_COMPONENT      f32
        GL_DEPTH24_STENCIL8        GL_DEPTH_STENCIL         24        8
        GL_STENCIL_INDEX8          GL_STENCIL_INDEX          8
        GL_DEPTH32F_STENCIL8       GL_DEPTH_STENCIL        f32        8
    + Compressed internal format
        Compressed Internal Format        Base Internal Format        Type
        GL_COMPRESSED_RED                        GL_RED              Generic
        GL_COMPRESSED_RG                         GL_RG               Generic
        GL_COMPRESSED_RGB                        GL_RGB              Generic
        GL_COMPRESSED_RGBA                       GL_RGBA             Generic
        GL_COMPRESSED_SRGB                       GL_RGB              Generic
        GL_COMPRESSED_SRGB_ALPHA                 GL_RGBA             Generic
        GL_COMPRESSED_RED_RGTC1                  GL_RED              Specific
        GL_COMPRESSED_SIGNED_RED_RGTC1           GL_RED              Specific
        GL_COMPRESSED_RG_RGTC2                   GL_RG               Specific
        GL_COMPRESSED_SIGNED_RG_RGTC2            GL_RG               Specific
        GL_COMPRESSED_RGBA_BPTC_UNORM            GL_RGBA             Specific
        GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM      GL_RGBA             Specific
        GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT      GL_RGB              Specific
        GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT    GL_RGB              Specific
*/

    switch (format) {
    default:
        break;
    case Texture::Format::R8:
        return GL_R8;
    case Texture::Format::R8SN:
        return GL_R8_SNORM;
    case Texture::Format::R16:
        return GL_R16;
    case Texture::Format::R16SN:
        return GL_R16_SNORM;
    case Texture::Format::RG8:
        return GL_RG8;
    case Texture::Format::RG8SN:
        return GL_RG8_SNORM;
    case Texture::Format::RG16:
        return GL_RG16;
    case Texture::Format::RG16SN:
        return GL_RG16_SNORM;
    case Texture::Format::RGB8:
        return GL_RGB8;
    case Texture::Format::RGB8SN:
        return GL_RGB8_SNORM;
    case Texture::Format::RGB16:
        return GL_RGB16;
    case Texture::Format::RGB16SN:
        return GL_RGB16_SNORM;
    case Texture::Format::RGBA8:
        return GL_RGBA8;
    case Texture::Format::RGBA8SN:
        return GL_RGBA8_SNORM;
    case Texture::Format::RGBA16:
        return GL_RGBA16;
    case Texture::Format::RGBA16SN:
        return GL_RGBA16_SNORM;
    case Texture::Format::R16F:
        return GL_R16F;
    case Texture::Format::RG16F:
        return GL_RG16F;
    case Texture::Format::RGB16F:
        return GL_RGB16F;
    case Texture::Format::RGBA16F:
        return GL_RGBA16F;
    case Texture::Format::R32F:
        return GL_R32F;
    case Texture::Format::RG32F:
        return GL_RG32F;
    case Texture::Format::RGB32F:
        return GL_RGB32F;
    case Texture::Format::RGBA32F:
        return GL_RGBA32F;
    case Texture::Format::R8I:
        return GL_R8I;
    case Texture::Format::R8UI:
        return GL_R8UI;
    case Texture::Format::R16I:
        return GL_R16I;
    case Texture::Format::R16UI:
        return GL_R16UI;
    case Texture::Format::R32I:
        return GL_R32I;
    case Texture::Format::R32UI:
        return GL_R32UI;
    case Texture::Format::RG8I:
        return GL_RG8I;
    case Texture::Format::RG8UI:
        return GL_RG8UI;
    case Texture::Format::RG16I:
        return GL_RG16I;
    case Texture::Format::RG16UI:
        return GL_RG16UI;
    case Texture::Format::RG32I:
        return GL_RG32I;
    case Texture::Format::RG32UI:
        return GL_RG32UI;
    case Texture::Format::RGB8I:
        return GL_RGB8I;
    case Texture::Format::RGB8UI:
        return GL_RGB8UI;
    case Texture::Format::RGB16I:
        return GL_RGB16I;
    case Texture::Format::RGB16UI:
        return GL_RGB16UI;
    case Texture::Format::RGB32I:
        return GL_RGB32I;
    case Texture::Format::RGB32UI:
        return GL_RGB32UI;
    case Texture::Format::RGBA8I:
        return GL_RGBA8I;
    case Texture::Format::RGBA8UI:
        return GL_RGBA8UI;
    case Texture::Format::RGBA16I:
        return GL_RGBA16I;
    case Texture::Format::RGBA16UI:
        return GL_RGBA16UI;
    case Texture::Format::RGBA32I:
        return GL_RGBA32I;
    case Texture::Format::RGBA32UI:
        return GL_RGBA32UI;
    case Texture::Format::SRGB8:
        return GL_SRGB8;
    case Texture::Format::SRGBA8:
        return GL_SRGB8_ALPHA8;
    case Texture::Format::R3G3B2:
        return GL_R3_G3_B2;
    case Texture::Format::RGB4:
        return GL_RGB4;
    case Texture::Format::RGB5:
        return GL_RGB5;
    case Texture::Format::RGB10:
        return GL_RGB10;
    case Texture::Format::RGB12:
        return GL_RGB12;
    case Texture::Format::RGBA2:
        return GL_RGBA2;
    case Texture::Format::RGBA4:
        return GL_RGBA4;
    case Texture::Format::RGB5A1:
        return GL_RGB5_A1;
    case Texture::Format::RGB10A2:
        return GL_RGB10_A2;
    case Texture::Format::RGB10A2UI:
        return GL_RGB10_A2UI;
    case Texture::Format::RGBA12:
        return GL_RGBA12;
    case Texture::Format::R11G11B10F:
        return GL_R11F_G11F_B10F;
    case Texture::Format::RGB9E5:
        return GL_RGB9_E5;
    case Texture::Format::Depth16:
        return GL_DEPTH_COMPONENT16;
    case Texture::Format::Depth24:
        return GL_DEPTH_COMPONENT24;
    case Texture::Format::Depth32:
        return GL_DEPTH_COMPONENT32;
    case Texture::Format::Depth32F:
        return GL_DEPTH_COMPONENT32F;
    case Texture::Format::Depth24Stencil8:
        return GL_DEPTH24_STENCIL8;
    case Texture::Format::Depth32FStencil8:
        return GL_DEPTH32F_STENCIL8;
    case Texture::Format::Stencil8:
        return GL_STENCIL_INDEX8;
    case Texture::Format::CompressRed:
        return GL_COMPRESSED_RED;
    case Texture::Format::CompressRG:
        return GL_COMPRESSED_RG;
    case Texture::Format::CompressRGB:
        return GL_COMPRESSED_RGB;
    case Texture::Format::CompressRGBA:
        return GL_COMPRESSED_RGBA;
    case Texture::Format::CompressSRGB:
        return GL_COMPRESSED_SRGB;
    case Texture::Format::CompressSRGBA:
        return GL_COMPRESSED_SRGB_ALPHA;
    case Texture::Format::CompressRed_RGTC1:
        return GL_COMPRESSED_RED_RGTC1;
    case Texture::Format::CompressSRed_RGTC1:
        return GL_COMPRESSED_SIGNED_RED_RGTC1;
    case Texture::Format::CompressRG_RGTC2:
        return GL_COMPRESSED_RG_RGTC2;
    case Texture::Format::CompressSRG_RGTC2:
        return GL_COMPRESSED_SIGNED_RG_RGTC2;
    case Texture::Format::CompressRGBA_BPTC_UN:
        return GL_COMPRESSED_RGBA_BPTC_UNORM;
    case Texture::Format::CompressSRGBA_BPTC_UN:
        return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
    case Texture::Format::CompressRGB_BPTC_SF:
        return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
    case Texture::Format::CompressRGB_BPTC_UF:
        return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
    }

    throw std::runtime_error("Invalid internal texture format");
}

static GLenum toNativeDataType(Texture::PixelType type) {
    switch (type) {
    default:
        break;
    case Texture::PixelType::UByte:
        return GL_UNSIGNED_BYTE;
    case Texture::PixelType::Byte:
        return GL_BYTE;
    case Texture::PixelType::UShort:
        return GL_UNSIGNED_SHORT;
    case Texture::PixelType::Short:
        return GL_SHORT;
    case Texture::PixelType::UInt:
        return GL_UNSIGNED_INT;
    case Texture::PixelType::Int:
        return GL_INT;
    case Texture::PixelType::HalfFloat:
        return GL_HALF_FLOAT;
    case Texture::PixelType::Float:
        return GL_FLOAT;
    case Texture::PixelType::UByte3_3_2:
        return GL_UNSIGNED_BYTE_3_3_2;
    case Texture::PixelType::UByte2_3_3_REV:
        return GL_UNSIGNED_BYTE_2_3_3_REV;
    case Texture::PixelType::UShort5_6_5:
        return GL_UNSIGNED_SHORT_5_6_5;
    case Texture::PixelType::UShort4_4_4_4:
        return GL_UNSIGNED_SHORT_4_4_4_4;
    case Texture::PixelType::UShort4_4_4_4_REV:
        return GL_UNSIGNED_SHORT_4_4_4_4_REV;
    case Texture::PixelType::UShort5_5_5_1:
        return GL_UNSIGNED_SHORT_5_5_5_1;
    case Texture::PixelType::UShort1_5_5_5_REV:
        return GL_UNSIGNED_SHORT_1_5_5_5_REV;
    case Texture::PixelType::UInt8_8_8_8:
        return GL_UNSIGNED_INT_8_8_8_8;
    case Texture::PixelType::UInt8_8_8_8_REV:
        return GL_UNSIGNED_INT_8_8_8_8_REV;
    case Texture::PixelType::UInt10_10_10_2:
        return GL_UNSIGNED_INT_10_10_10_2;
    case Texture::PixelType::UInt2_10_10_10_REV:
        return GL_UNSIGNED_INT_2_10_10_10_REV;
    case Texture::PixelType::UInt24_8:
        return GL_UNSIGNED_INT_24_8;
    case Texture::PixelType::UInt5_9_9_9_REV:
        return GL_UNSIGNED_INT_5_9_9_9_REV;
    case Texture::PixelType::B10G11R11F_REV:
        return GL_UNSIGNED_INT_10F_11F_11F_REV;
    case Texture::PixelType::Float32_UInt24_8_REV:
        return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
    }

    throw std::runtime_error("Invalid texture pixel type");
}

static size_t getComponents(Texture::ExternalFormat format) {
    switch (format) {
    default:
        break;
    case Texture::ExternalFormat::Red:
        return 1;
    case Texture::ExternalFormat::RG:
        return 2;
    case Texture::ExternalFormat::RGB:
        return 3;
    case Texture::ExternalFormat::BGR:
        return 3;
    case Texture::ExternalFormat::RGBA:
        return 4;
    case Texture::ExternalFormat::BGRA:
        return 4;
    case Texture::ExternalFormat::RedInt:
        return 1;
    case Texture::ExternalFormat::RGInt:
        return 2;
    case Texture::ExternalFormat::RGBInt:
        return 3;
    case Texture::ExternalFormat::BGRInt:
        return 3;
    case Texture::ExternalFormat::RGBAInt:
        return 4;
    case Texture::ExternalFormat::BGRAInt:
        return 4;
    case Texture::ExternalFormat::Depth:
        return 1;
    case Texture::ExternalFormat::Stencil:
        return 1;
        // always be considered as combined @ref getComponentSize(Texture::PixelType::Float32_UInt24_8)
    case Texture::ExternalFormat::DepthStencil:
        return 1;
    }

    throw std::runtime_error("Invalid external texture format");
}

static bool isPackedPixelType(Texture::PixelType type) {
    switch (type) {
    default:
        break;
    case Texture::PixelType::Byte:
    case Texture::PixelType::UByte:
    case Texture::PixelType::UShort:
    case Texture::PixelType::Short:
    case Texture::PixelType::UInt:
    case Texture::PixelType::Int:
    case Texture::PixelType::HalfFloat:
    case Texture::PixelType::Float:
        return false;
    case Texture::PixelType::UByte3_3_2:
    case Texture::PixelType::UByte2_3_3_REV:
    case Texture::PixelType::UShort5_6_5:
    case Texture::PixelType::UShort5_6_5_REV:
    case Texture::PixelType::UShort4_4_4_4:
    case Texture::PixelType::UShort4_4_4_4_REV:
    case Texture::PixelType::UShort5_5_5_1:
    case Texture::PixelType::UShort1_5_5_5_REV:
    case Texture::PixelType::UInt8_8_8_8:
    case Texture::PixelType::UInt8_8_8_8_REV:
    case Texture::PixelType::UInt10_10_10_2:
    case Texture::PixelType::UInt2_10_10_10_REV:
    case Texture::PixelType::UInt24_8:
    case Texture::PixelType::UInt5_9_9_9_REV:
    case Texture::PixelType::B10G11R11F_REV:
    case Texture::PixelType::Float32_UInt24_8_REV:
        return true;
    }

    throw std::runtime_error("Invalid texture pixel type");
}

static size_t getComponentSize(Texture::PixelType type) {
    switch (type) {
    default:
        break;
    case Texture::PixelType::Byte:
        [[fallthrough]];
    case Texture::PixelType::UByte3_3_2:
        [[fallthrough]];
    case Texture::PixelType::UByte2_3_3_REV:
        [[fallthrough]];
    case Texture::PixelType::UByte:
        return 1;
    case Texture::PixelType::UShort:
        [[fallthrough]];
    case Texture::PixelType::Short:
        [[fallthrough]];
    case Texture::PixelType::UShort5_6_5:
        [[fallthrough]];
    case Texture::PixelType::UShort5_6_5_REV:
        [[fallthrough]];
    case Texture::PixelType::UShort4_4_4_4:
        [[fallthrough]];
    case Texture::PixelType::UShort4_4_4_4_REV:
        [[fallthrough]];
    case Texture::PixelType::UShort5_5_5_1:
        [[fallthrough]];
    case Texture::PixelType::UShort1_5_5_5_REV:
        [[fallthrough]];
    case Texture::PixelType::HalfFloat:
        return 2;
    case Texture::PixelType::UInt:
        [[fallthrough]];
    case Texture::PixelType::Int:
        [[fallthrough]];
    case Texture::PixelType::UInt8_8_8_8:
        [[fallthrough]];
    case Texture::PixelType::UInt8_8_8_8_REV:
        [[fallthrough]];
    case Texture::PixelType::UInt10_10_10_2:
        [[fallthrough]];
    case Texture::PixelType::UInt2_10_10_10_REV:
        [[fallthrough]];
    case Texture::PixelType::UInt24_8:
        [[fallthrough]];
    case Texture::PixelType::UInt5_9_9_9_REV:
        [[fallthrough]];
    case Texture::PixelType::B10G11R11F_REV:
        [[fallthrough]];
    case Texture::PixelType::Float:
        return 4;
    case Texture::PixelType::Float32_UInt24_8_REV:
        return 8;
    }

    throw std::runtime_error("Invalid texture pixel type");
}

static size_t getPixelSize(Texture::ExternalFormat format, Texture::PixelType type) {
    size_t componentSize{getComponentSize(type)};
    if (isPackedPixelType(type)) {
        return componentSize;
    }

    return componentSize * getComponents(format);
}

static GLenum getImageFormat(Texture::Format format) {
    /*
     * @ref https://docs.gl/gl4/glBindImageTexture
     * format specifies the format used when performing formatted stores into the image from shaders.
     * format must be compatible with the texture's internal format and must be one of the formats.
       Image Unit Format     Format Qualifier
        GL_RGBA32F              rgba32f
        GL_RGBA16F              rgba16f
        GL_RG32F                rg32f
        GL_RG16F                rg16f
        GL_R32F                 r32f
        GL_R16F                 r16f
        GL_RGBA32UI             rgba32ui
        GL_RGBA16UI             rgba16ui
        GL_RGBA8UI              rgba8ui
        GL_RG32UI               rg32ui
        GL_RG16UI               rg16ui
        GL_RG8UI                rg8ui
        GL_R32UI                r32ui
        GL_R16UI                r16ui
        GL_R8UI                 r8ui
        GL_RGBA32I              rgba32i
        GL_RGBA16I              rgba16i
        GL_RGBA8I               rgba8i
        GL_RG32I                rg32i
        GL_RG16I                rg16i
        GL_RG8I                 rg8i
        GL_R32I                 r32i
        GL_R16I                 r16i
        GL_R8I                  r8i
        GL_RGBA16               rgba16
        GL_RGBA8                rgba8
        GL_RG16                 rg16
        GL_RG8                  rg8
        GL_R16                  r16
        GL_R8                   r8
        // not used
        GL_RGB10_A2UI           rgb10_a2ui
        GL_R11F_G11F_B10F       r11f_g11f_b10f
        GL_RGB10_A2             rgb10_a2
        GL_RGBA16_SNORM         rgba16_snorm
        GL_RGBA8_SNORM          rgba8_snorm
        GL_RG16_SNORM           rg16_snorm
        GL_RG8_SNORM            rg8_snorm
        GL_R16_SNORM            r16_snorm
        GL_R8_SNORM             r8_snorm
     */

    switch (format) {
    default:
        break;
    case Texture::Format::R8:
        return GL_R8;
    case Texture::Format::R16:
        return GL_R16;
    case Texture::Format::RG8:
        return GL_RG8;
    case Texture::Format::RG16:
        return GL_RG16;
    case Texture::Format::RGB8:
        return GL_RGB8;
    case Texture::Format::RGB16:
        return GL_RGB16;
    case Texture::Format::RGBA8:
        return GL_RGBA8;
    case Texture::Format::RGBA16:
        return GL_RGBA16;
    case Texture::Format::R16F:
        return GL_R16F;
    case Texture::Format::RG16F:
        return GL_RG16F;
    case Texture::Format::RGB16F:
        return GL_RGB16F;
    case Texture::Format::RGBA16F:
        return GL_RGBA16F;
    case Texture::Format::R32F:
        return GL_R32F;
    case Texture::Format::RG32F:
        return GL_RG32F;
    case Texture::Format::RGB32F:
        return GL_RGB32F;
    case Texture::Format::RGBA32F:
        return GL_RGBA32F;
    case Texture::Format::R8I:
        return GL_R8I;
    case Texture::Format::R8UI:
        return GL_R8UI;
    case Texture::Format::R16I:
        return GL_R16I;
    case Texture::Format::R16UI:
        return GL_R16UI;
    case Texture::Format::R32I:
        return GL_R32I;
    case Texture::Format::R32UI:
        return GL_R32UI;
    case Texture::Format::RG8I:
        return GL_RG8I;
    case Texture::Format::RG8UI:
        return GL_RG8UI;
    case Texture::Format::RG16I:
        return GL_RG16I;
    case Texture::Format::RG16UI:
        return GL_RG16UI;
    case Texture::Format::RG32I:
        return GL_RG32I;
    case Texture::Format::RG32UI:
        return GL_RG32UI;
    case Texture::Format::RGB8I:
        return GL_RGB8I;
    case Texture::Format::RGB8UI:
        return GL_RGB8UI;
    case Texture::Format::RGB16I:
        return GL_RGB16I;
    case Texture::Format::RGB16UI:
        return GL_RGB16UI;
    case Texture::Format::RGB32I:
        return GL_RGB32I;
    case Texture::Format::RGB32UI:
        return GL_RGB32UI;
    case Texture::Format::RGBA8I:
        return GL_RGBA8I;
    case Texture::Format::RGBA8UI:
        return GL_RGBA8UI;
    case Texture::Format::RGBA16I:
        return GL_RGBA16I;
    case Texture::Format::RGBA16UI:
        return GL_RGBA16UI;
    case Texture::Format::RGBA32I:
        return GL_RGBA32I;
    case Texture::Format::RGBA32UI:
        return GL_RGBA32UI;
    }

    throw std::runtime_error("Cannot find a compatible image format from the internal format");
}

static GLenum getImageAccess(Texture::Access access) {
    switch (access) {
    default:
        break;
    case Texture::Access::ReadOnly:
        return GL_READ_ONLY;
    case Texture::Access::WriteOnly:
        return GL_WRITE_ONLY;
    case Texture::Access::ReadWrite:
        return GL_READ_WRITE;
    }

    throw std::runtime_error("Unsupported texture access mode");
}

static Texture::Format chooseCompatibleFormat(Texture::ExternalFormat format, Texture::PixelType type) {
    // TODO: Consider _SNORM type for signed data type
    switch (format) {
    default:
        break;
    case Texture::ExternalFormat::Red:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::R8;
        case Texture::PixelType::Byte:
            return Texture::Format::R8SN;
        case Texture::PixelType::UShort:
            return Texture::Format::R16;
        case Texture::PixelType::Short:
            return Texture::Format::R16SN;
        case Texture::PixelType::HalfFloat:
            return Texture::Format::R16F;
        case Texture::PixelType::Float:
            return Texture::Format::R32F;
        }
        break;
    case Texture::ExternalFormat::RG:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::RG8;
        case Texture::PixelType::Byte:
            return Texture::Format::RG8SN;
        case Texture::PixelType::UShort:
            return Texture::Format::RG16;
        case Texture::PixelType::Short:
            return Texture::Format::RG16SN;
        case Texture::PixelType::HalfFloat:
            return Texture::Format::RG16F;
        case Texture::PixelType::Float:
            return Texture::Format::RG32F;
        }
        break;
    case Texture::ExternalFormat::RGB:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::RGB8;
        case Texture::PixelType::Byte:
            return Texture::Format::RGB8SN;
        case Texture::PixelType::UShort:
            return Texture::Format::RGB16;
        case Texture::PixelType::Short:
            return Texture::Format::RGB16SN;
        case Texture::PixelType::HalfFloat:
            return Texture::Format::RGB16F;
        case Texture::PixelType::Float:
            return Texture::Format::RGB32F;
        case Texture::PixelType::UByte3_3_2:
            [[fallthrough]];
        case Texture::PixelType::UByte2_3_3_REV:
            return Texture::Format::R3G3B2;
        case Texture::PixelType::UShort5_6_5:
            [[fallthrough]];
        case Texture::PixelType::UShort5_6_5_REV:
            return Texture::Format::RGB8;
        case Texture::PixelType::B10G11R11F_REV:
            return Texture::Format::R11G11B10F;
        }
        break;
    case Texture::ExternalFormat::RGBA:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::RGBA8;
        case Texture::PixelType::Byte:
            return Texture::Format::RGBA8SN;
        case Texture::PixelType::UShort:
            return Texture::Format::RGBA16;
        case Texture::PixelType::Short:
            return Texture::Format::RGBA16SN;
        case Texture::PixelType::HalfFloat:
            return Texture::Format::RGBA16F;
        case Texture::PixelType::Float:
            return Texture::Format::RGBA32F;
        case Texture::PixelType::UShort4_4_4_4:
            [[fallthrough]];
        case Texture::PixelType::UShort4_4_4_4_REV:
            return Texture::Format::RGBA4;
        case Texture::PixelType::UShort5_5_5_1:
            [[fallthrough]];
        case Texture::PixelType::UShort1_5_5_5_REV:
            return Texture::Format::RGB5A1;
        case Texture::PixelType::UInt8_8_8_8:
            [[fallthrough]];
        case Texture::PixelType::UInt8_8_8_8_REV:
            return Texture::Format::RGBA8;
        case Texture::PixelType::UInt5_9_9_9_REV:
            return Texture::Format::RGB9E5;
        case Texture::PixelType::UInt10_10_10_2:
            [[fallthrough]];
        case Texture::PixelType::UInt2_10_10_10_REV:
            return Texture::Format::RGB10A2UI;
        }
        break;
    case Texture::ExternalFormat::RedInt:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::R8UI;
        case Texture::PixelType::Byte:
            return Texture::Format::R8I;
        case Texture::PixelType::UShort:
            return Texture::Format::R16UI;
        case Texture::PixelType::Short:
            return Texture::Format::R16I;
        case Texture::PixelType::UInt:
            return Texture::Format::R32UI;
        case Texture::PixelType::Int:
            return Texture::Format::R32I;
        }
        break;
    case Texture::ExternalFormat::RGInt:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::RG8UI;
        case Texture::PixelType::Byte:
            return Texture::Format::RG8I;
        case Texture::PixelType::UShort:
            return Texture::Format::RG16UI;
        case Texture::PixelType::Short:
            return Texture::Format::RG16I;
        case Texture::PixelType::UInt:
            return Texture::Format::RG32UI;
        case Texture::PixelType::Int:
            return Texture::Format::RG32I;
        }
        break;
    case Texture::ExternalFormat::RGBInt:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::RGB8UI;
        case Texture::PixelType::Byte:
            return Texture::Format::RGB8I;
        case Texture::PixelType::UShort:
            return Texture::Format::RGB16UI;
        case Texture::PixelType::Short:
            return Texture::Format::RGB16I;
        case Texture::PixelType::UInt:
            return Texture::Format::RGB32UI;
        case Texture::PixelType::Int:
            return Texture::Format::RGB32I;
        }
        break;
    case Texture::ExternalFormat::RGBAInt:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::RGBA8UI;
        case Texture::PixelType::Byte:
            return Texture::Format::RGBA8I;
        case Texture::PixelType::UShort:
            return Texture::Format::RGBA16UI;
        case Texture::PixelType::Short:
            return Texture::Format::RGBA16I;
        case Texture::PixelType::UInt:
            return Texture::Format::RGBA32UI;
        case Texture::PixelType::Int:
            return Texture::Format::RGBA32I;
        }
        break;
    case Texture::ExternalFormat::Depth:
        switch (type) {
        default:
            break;
        case Texture::PixelType::HalfFloat:
            return Texture::Format::Depth16;
            // https://gamedev.stackexchange.com/questions/168241/is-gl-depth-component32-deprecated-in-opengl-4-5
        case Texture::PixelType::Float:
            return Texture::Format::Depth32F;
        }
        break;
    case Texture::ExternalFormat::Stencil:
        switch (type) {
        default:
            break;
        case Texture::PixelType::UByte:
            return Texture::Format::Stencil8;
        }
        break;
    case Texture::ExternalFormat::DepthStencil:
        switch (type) {
        default:
            break;
        case Texture::PixelType::Float:
            return Texture::Format::Depth24Stencil8;
        case Texture::PixelType::Float32_UInt24_8_REV:
            return Texture::Format::Depth32FStencil8;
        }
        break;
    }

    throw std::runtime_error("Cannot determine the internal format from the external format and pixel type");
}

static Texture::ExternalFormat chooseCompatibleExternalFormat(Texture::Format format) {
    switch (format) {
    default:
        break;
    case Texture::Format::R8:
        [[fallthrough]];
    case Texture::Format::R8SN:
        [[fallthrough]];
    case Texture::Format::R16:
        [[fallthrough]];
    case Texture::Format::R16SN:
        [[fallthrough]];
    case Texture::Format::R16F:
        [[fallthrough]];
    case Texture::Format::R32F:
        [[fallthrough]];
    case Texture::Format::CompressRed:
        [[fallthrough]];
    case Texture::Format::CompressRed_RGTC1:
        [[fallthrough]];
    case Texture::Format::CompressSRed_RGTC1:
        return Texture::ExternalFormat::Red;
    case Texture::Format::RG8:
        [[fallthrough]];
    case Texture::Format::RG8SN:
        [[fallthrough]];
    case Texture::Format::RG16:
        [[fallthrough]];
    case Texture::Format::RG16SN:
        [[fallthrough]];
    case Texture::Format::RG16F:
        [[fallthrough]];
    case Texture::Format::RG32F:
        [[fallthrough]];
    case Texture::Format::CompressRG:
        [[fallthrough]];
    case Texture::Format::CompressRG_RGTC2:
        [[fallthrough]];
    case Texture::Format::CompressSRG_RGTC2:
        return Texture::ExternalFormat::RG;
    case Texture::Format::RGB8:
        [[fallthrough]];
    case Texture::Format::RGB8SN:
        [[fallthrough]];
    case Texture::Format::RGB16:
        [[fallthrough]];
    case Texture::Format::RGB16SN:
        [[fallthrough]];
    case Texture::Format::RGB16F:
        [[fallthrough]];
    case Texture::Format::RGB32F:
        [[fallthrough]];
    case Texture::Format::SRGB8:
        [[fallthrough]];
    case Texture::Format::R3G3B2:
        [[fallthrough]];
    case Texture::Format::RGB4:
        [[fallthrough]];
    case Texture::Format::RGB5:
        [[fallthrough]];
    case Texture::Format::RGB10:
        [[fallthrough]];
    case Texture::Format::RGB12:
        [[fallthrough]];
    case Texture::Format::R11G11B10F:
        [[fallthrough]];
    case Texture::Format::CompressRGB:
        [[fallthrough]];
    case Texture::Format::CompressSRGB:
        [[fallthrough]];
    case Texture::Format::CompressRGB_BPTC_SF:
        [[fallthrough]];
    case Texture::Format::CompressRGB_BPTC_UF:
        return Texture::ExternalFormat::RGB;
    case Texture::Format::RGBA8:
        [[fallthrough]];
    case Texture::Format::RGBA8SN:
        [[fallthrough]];
    case Texture::Format::RGBA16:
        [[fallthrough]];
    case Texture::Format::RGBA16SN:
        [[fallthrough]];
    case Texture::Format::RGBA16F:
        [[fallthrough]];
    case Texture::Format::RGBA32F:
        [[fallthrough]];
    case Texture::Format::SRGBA8:
        [[fallthrough]];
    case Texture::Format::RGBA2:
        [[fallthrough]];
    case Texture::Format::RGBA4:
        [[fallthrough]];
    case Texture::Format::RGB5A1:
        [[fallthrough]];
    case Texture::Format::RGB10A2:
        [[fallthrough]];
    case Texture::Format::RGB10A2UI:
        [[fallthrough]];
    case Texture::Format::RGBA12:
        [[fallthrough]];
    case Texture::Format::RGB9E5:
        [[fallthrough]];
    case Texture::Format::CompressRGBA:
        [[fallthrough]];
    case Texture::Format::CompressSRGBA:
        [[fallthrough]];
    case Texture::Format::CompressRGBA_BPTC_UN:
        [[fallthrough]];
    case Texture::Format::CompressSRGBA_BPTC_UN:
        return Texture::ExternalFormat::RGBA;
    case Texture::Format::R8I:
        [[fallthrough]];
    case Texture::Format::R8UI:
        [[fallthrough]];
    case Texture::Format::R16I:
        [[fallthrough]];
    case Texture::Format::R16UI:
        [[fallthrough]];
    case Texture::Format::R32I:
        [[fallthrough]];
    case Texture::Format::R32UI:
        return Texture::ExternalFormat::RedInt;
    case Texture::Format::RG8I:
        [[fallthrough]];
    case Texture::Format::RG8UI:
        [[fallthrough]];
    case Texture::Format::RG16I:
        [[fallthrough]];
    case Texture::Format::RG16UI:
        [[fallthrough]];
    case Texture::Format::RG32I:
        [[fallthrough]];
    case Texture::Format::RG32UI:
        return Texture::ExternalFormat::RGInt;
    case Texture::Format::RGB8I:
        [[fallthrough]];
    case Texture::Format::RGB8UI:
        [[fallthrough]];
    case Texture::Format::RGB16I:
        [[fallthrough]];
    case Texture::Format::RGB16UI:
        [[fallthrough]];
    case Texture::Format::RGB32I:
        [[fallthrough]];
    case Texture::Format::RGB32UI:
        return Texture::ExternalFormat::RGBInt;
    case Texture::Format::RGBA8I:
        [[fallthrough]];
    case Texture::Format::RGBA8UI:
        [[fallthrough]];
    case Texture::Format::RGBA16I:
        [[fallthrough]];
    case Texture::Format::RGBA16UI:
        [[fallthrough]];
    case Texture::Format::RGBA32I:
        [[fallthrough]];
    case Texture::Format::RGBA32UI:
        return Texture::ExternalFormat::RGBAInt;
    case Texture::Format::Depth16:
        [[fallthrough]];
    case Texture::Format::Depth24:
        [[fallthrough]];
    case Texture::Format::Depth32:
        [[fallthrough]];
    case Texture::Format::Depth32F:
        return Texture::ExternalFormat::Depth;
    case Texture::Format::Stencil8:
        return Texture::ExternalFormat::Stencil;
    case Texture::Format::Depth24Stencil8:
        [[fallthrough]];
    case Texture::Format::Depth32FStencil8:
        return Texture::ExternalFormat::DepthStencil;
    }

    throw std::runtime_error("Cannot choose an external format from the internal format");
}

static Texture::PixelType chooseCompatiblePixelType(Texture::Format format) {
    switch (format) {
    default:
        break;
    case Texture::Format::R8:
        [[fallthrough]];
    case Texture::Format::RG8:
        [[fallthrough]];
    case Texture::Format::RGB8:
        [[fallthrough]];
    case Texture::Format::RGBA8:
        [[fallthrough]];
    case Texture::Format::R8UI:
        [[fallthrough]];
    case Texture::Format::RG8UI:
        [[fallthrough]];
    case Texture::Format::RGB8UI:
        [[fallthrough]];
    case Texture::Format::RGBA8UI:
        [[fallthrough]];
    case Texture::Format::SRGB8:
        [[fallthrough]];
    case Texture::Format::SRGBA8:
        [[fallthrough]];
    case Texture::Format::Stencil8:
        return Texture::PixelType::UByte;
    case Texture::Format::RG8SN:
        [[fallthrough]];
    case Texture::Format::R8SN:
        [[fallthrough]];
    case Texture::Format::RGB8SN:
        [[fallthrough]];
    case Texture::Format::RGBA8SN:
        [[fallthrough]];
    case Texture::Format::R8I:
        [[fallthrough]];
    case Texture::Format::RG8I:
        [[fallthrough]];
    case Texture::Format::RGB8I:
        [[fallthrough]];
    case Texture::Format::RGBA8I:
        return Texture::PixelType::Byte;
    case Texture::Format::R16SN:
        [[fallthrough]];
    case Texture::Format::RG16SN:
        [[fallthrough]];
    case Texture::Format::RGB16SN:
        [[fallthrough]];
    case Texture::Format::RGBA16SN:
        [[fallthrough]];
    case Texture::Format::R16I:
        [[fallthrough]];
    case Texture::Format::RG16I:
        [[fallthrough]];
    case Texture::Format::RGB16I:
        [[fallthrough]];
    case Texture::Format::RGBA16I:
        return Texture::PixelType::Short;
    case Texture::Format::R16:
        [[fallthrough]];
    case Texture::Format::RG16:
        [[fallthrough]];
    case Texture::Format::RGB16:
        [[fallthrough]];
    case Texture::Format::RGBA16:
        [[fallthrough]];
    case Texture::Format::R16UI:
        [[fallthrough]];
    case Texture::Format::RG16UI:
        [[fallthrough]];
    case Texture::Format::RGB16UI:
        [[fallthrough]];
    case Texture::Format::RGBA16UI:
        return Texture::PixelType::UShort;
    case Texture::Format::R16F:
        [[fallthrough]];
    case Texture::Format::RG16F:
        [[fallthrough]];
    case Texture::Format::RGB16F:
        [[fallthrough]];
    case Texture::Format::RGBA16F:
        [[fallthrough]];
    case Texture::Format::Depth16:
        return Texture::PixelType::HalfFloat;
    case Texture::Format::R32F:
        [[fallthrough]];
    case Texture::Format::RG32F:
        [[fallthrough]];
    case Texture::Format::RGB32F:
        [[fallthrough]];
    case Texture::Format::RGBA32F:
        [[fallthrough]];
    case Texture::Format::Depth24:
        [[fallthrough]];
    case Texture::Format::Depth32:
        [[fallthrough]];
    case Texture::Format::Depth32F:
        [[fallthrough]];
    case Texture::Format::Depth24Stencil8:
        return Texture::PixelType::Float;
    case Texture::Format::R32I:
        [[fallthrough]];
    case Texture::Format::RG32I:
        [[fallthrough]];
    case Texture::Format::RGB32I:
        [[fallthrough]];
    case Texture::Format::RGBA32I:
        return Texture::PixelType::Int;
    case Texture::Format::R32UI:
        [[fallthrough]];
    case Texture::Format::RG32UI:
        [[fallthrough]];
    case Texture::Format::RGB32UI:
        [[fallthrough]];
    case Texture::Format::RGBA32UI:
        return Texture::PixelType::UInt;
    case Texture::Format::R3G3B2:
        return Texture::PixelType::UByte3_3_2;
    case Texture::Format::RGB4:
        return Texture::PixelType::UByte;
    case Texture::Format::RGB5:
        return Texture::PixelType::UShort5_6_5;
    case Texture::Format::RGB10:
        return Texture::PixelType::UInt10_10_10_2;
    case Texture::Format::RGB12:
        return Texture::PixelType::UShort;
    case Texture::Format::RGBA2:
        return Texture::PixelType::UByte;
    case Texture::Format::RGBA4:
        return Texture::PixelType::UShort4_4_4_4;
    case Texture::Format::RGB5A1:
        return Texture::PixelType::UShort5_5_5_1;
    case Texture::Format::RGB10A2:
        return Texture::PixelType::UInt10_10_10_2;
    case Texture::Format::RGB10A2UI:
        return Texture::PixelType::UInt10_10_10_2;
    case Texture::Format::RGBA12:
        return Texture::PixelType::Short;
    case Texture::Format::R11G11B10F:
        return Texture::PixelType::B10G11R11F_REV;
    case Texture::Format::RGB9E5:
        return Texture::PixelType::UInt5_9_9_9_REV;
    case Texture::Format::Depth32FStencil8:
        return Texture::PixelType::Float32_UInt24_8_REV;
        // Compressed image should be specified with external format(see glCompressedTex...)
        // case Texture::Format::CompressRed:           return Texture::PixelType::None;
        // case Texture::Format::CompressRG :           return Texture::PixelType::None;
        // case Texture::Format::CompressRGB :          return Texture::PixelType::None;
        // case Texture::Format::CompressRGBA :         return Texture::PixelType::None;
        // case Texture::Format::CompressSRGB :         return Texture::PixelType::None;
        // case Texture::Format::CompressSRGBA :        return Texture::PixelType::None;
        // case Texture::Format::CompressRed_RGTC1:     return Texture::PixelType::None;
        // case Texture::Format::CompressSRed_RGTC1:    return Texture::PixelType::None;
        // case Texture::Format::CompressRG_RGTC2 :     return Texture::PixelType::None;
        // case Texture::Format::CompressSRG_RGTC2 :    return Texture::PixelType::None;
        // case Texture::Format::CompressRGBA_BPTC_UN:  return Texture::PixelType::None;
        // case Texture::Format::CompressSRGBA_BPTC_UN: return Texture::PixelType::None;
        // case Texture::Format::CompressRGB_BPTC_SF:   return Texture::PixelType::None;
        // case Texture::Format::CompressRGB_BPTC_UF:   return Texture::PixelType::None;
    }

    throw std::runtime_error("Cannot choose a pixel type from the internal format");
}

static uint32_t getMaxMipmapLevels(uint32_t width, uint32_t height) {
    uint32_t maxMipmapLevel{0};
    for (uint32_t extent = std::max(width, height); extent != 1; extent /= 2) {
        ++maxMipmapLevel;
    }

    return 1 + maxMipmapLevel;
}
} // namespace details
} // namespace gfx
