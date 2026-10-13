#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>

class CpuTexture2D {
public:
    enum class Filter {
        // Select the closest texel without interpolation.
        Nearest,
        // Blend the four neighboring texels with bilinear interpolation.
        Linear,
        // Select the nearest texel from the nearest mip level.
        NearestMipmapNearest,
        // Bilinearly filter the nearest mip level.
        LinearMipmapNearest,
        // Blend nearest-texel lookups from the two surrounding mip levels.
        NearestMipmapLinear,
        // Trilinearly filter the two surrounding mip levels.
        LinearMipmapLinear
    };

    enum class AddressMode {
        // Tile the base level indefinitely.
        Repeat,
        // Reflect every second tile.
        MirroredRepeat,
        // Use the nearest edge texel outside the normalized coordinate range.
        ClampToEdge
    };

    enum class InternalFormat {
        // One normalized unsigned 8-bit component per texel.
        R8,
        // Two normalized unsigned 8-bit components per texel.
        RG8,
        // Three normalized unsigned 8-bit components per texel.
        RGB8,
        // Four normalized unsigned 8-bit components per texel.
        RGBA8,
        // One IEEE 754 half-precision component per texel.
        R16F,
        // Two IEEE 754 half-precision components per texel.
        RG16F,
        // Three IEEE 754 half-precision components per texel.
        RGB16F,
        // Four IEEE 754 half-precision components per texel.
        RGBA16F,
        // One IEEE 754 single-precision component per texel.
        R32F,
        // Two IEEE 754 single-precision components per texel.
        RG32F,
        // Three IEEE 754 single-precision components per texel.
        RGB32F,
        // Four IEEE 754 single-precision components per texel.
        RGBA32F
    };

    enum class Format {
        // One red component per texel.
        Red,
        // Red and green components per texel.
        RedGreen,
        // Red, green, and blue components per texel.
        RedGreenBlue,
        // Blue, green, and red source order per texel.
        BlueGreenRed,
        // Red, green, blue, and alpha components per texel.
        RedGreenBlueAlpha,
        // Blue, green, red, and alpha source order per texel.
        BlueGreenRedAlpha,
        // One depth component per texel.
        Depth
    };

    enum class DataType {
        // Eight-bit unsigned normalized component storage.
        UnsignedByte,
        // Eight-bit signed normalized component storage.
        Byte,
        // Sixteen-bit unsigned normalized component storage.
        UnsignedShort,
        // Sixteen-bit signed normalized component storage.
        Short,
        // IEEE 754 binary16 component storage.
        HalfFloat,
        // Thirty-two-bit unsigned normalized component storage.
        UnsignedInt,
        // Thirty-two-bit signed normalized component storage.
        Int,
        // IEEE 754 binary32 component storage.
        Float
    };


public:
    // Allocates internal-format storage and converts the optional source image into that storage.
    // A null source allocates deterministic zero-initialized storage of the requested size.
    CpuTexture2D(
        InternalFormat internalFormat, int width, int height, Format format, DataType dataType,
        const void* pixels = nullptr);

    ~CpuTexture2D();

    CpuTexture2D(CpuTexture2D&& rhs) noexcept;

    CpuTexture2D& operator=(CpuTexture2D&& rhs) noexcept;

    // Sets the addressing rule for the horizontal S texture coordinate.
    void setAddressModeS(AddressMode addressMode);

    // Sets the addressing rule for the vertical T texture coordinate.
    void setAddressModeT(AddressMode addressMode);

    // Sets the filter used by software lookups identified as minification.
    void setMinFilter(Filter filter);

    // Sets the filter used by software lookups identified as magnification.
    void setMagFilter(Filter filter);

    // Generates every lower-resolution level by box-filtering the internal base level. Replaces
    // any mip chain created by a previous call.
    void generatorMipMap();

    // Replaces every base-level texel by converting one complete external-format source image into
    // this texture's internal storage. The source layout must match format() and dataType().
    void updatePixels(const void* pixels);

    // Samples only the base mip level. Because texture-coordinate derivatives are not available,
    // this lookup uses the texel filter portion of the configured minification filter.
    glm::vec4 sample(const glm::vec2& uv) const;

    // Samples the generated mip chain at an explicit continuous level-of-detail value. This lookup
    // uses the configured minification filter; level zero is the base texture and positive values
    // select progressively smaller levels.
    glm::vec4 sampleMipMap(const glm::vec2& uv, float levelOfDetail) const;

    // Returns the allocated base-level width in texels.
    int width() const;

    // Returns the allocated base-level height in texels.
    int height() const;

    // Returns the internal representation used by CPU storage and texture sampling.
    InternalFormat internalFormat() const;

    // Returns the external texel-component layout expected by updatePixels().
    Format format() const;

    // Returns the scalar representation expected by updatePixels().
    DataType dataType() const;

    // Exposes level-zero internal-format storage so callers can update individual texels in place.
    void* pixels();

    // Exposes level-zero internal-format storage for read-only upload or inspection.
    const void* pixels() const;

    // Returns the canonical component layout of mip level zero for GPU texture upload.
    Format storageFormat() const;

    // Returns the scalar type of mip level zero for GPU texture upload.
    DataType storageDataType() const;

private:
    struct MipLevel {
        // Width of this mip level in texels.
        int width = 0;
        // Height of this mip level in texels.
        int height = 0;
        // Raw texel storage encoded with the same internal format as the base level.
        void* pixels = nullptr;
    };

private:
    // Every owned mip level, including the level-zero base texture at index zero.
    // This is a simple implementation of the gpu mip chain, which is not required to be contiguous in memory.
    // In GPU memory, the texture storage is optimized according to the internal format and whether the mipmap is required.
    std::vector<MipLevel> m_mipLevels;

    // Internal representation used to encode each mip level and decode samples.
    InternalFormat m_internalFormat = InternalFormat::RGBA8;

    // External component ordering used only while updatePixels() decodes its source image.
    Format m_format = Format::RedGreenBlueAlpha;

    // External scalar type used only while updatePixels() decodes its source image.
    DataType m_dataType = DataType::UnsignedByte;

    // Number of components per internally stored texel, derived once from m_internalFormat.
    size_t m_components = 0;

    // Byte width of one internally stored component, derived once from m_internalFormat.
    size_t m_bytesPerComponent = 0;

    // Addressing mode for the texture's horizontal S coordinate.
    AddressMode m_addressModeS = AddressMode::ClampToEdge;

    // Addressing mode for the texture's vertical T coordinate.
    AddressMode m_addressModeT = AddressMode::ClampToEdge;

    // Filter used when a software caller identifies the lookup as minification.
    Filter m_minFilter = Filter::Nearest;

    // Filter used when a software caller identifies the lookup as magnification.
    Filter m_magFilter = Filter::Nearest;

private:
    size_t byteSize() const;

    size_t levelByteSize(int width, int height) const;

    void clearMipLevels() noexcept;

    void clearGeneratedMipLevels() noexcept;

    glm::vec4 texelFromLevel(
        const void* pixels, int width, int height, int x, int y) const;

    glm::vec4 sampleLevel(const glm::vec2& uv, size_t level, Filter filter) const;

    glm::vec4 sourceTexel(const std::byte* source) const;

    void writeTexel(std::byte* destination, const glm::vec4& value) const;

    float sourceComponent(const std::byte* source) const;

    float storedComponent(const std::byte* source) const;

    static float halfToFloat(uint16_t bits) noexcept;

    static uint16_t floatToHalf(float value) noexcept;

    static int resolveCoordinate(int coordinate, int extent, AddressMode addressMode) noexcept;

    static float resolveUv(float coordinate, AddressMode addressMode) noexcept;

    static size_t componentCount(InternalFormat format);

    static size_t sourceComponentCount(Format format);

    static size_t bytesPerComponent(InternalFormat format);

    static size_t sourceBytesPerComponent(DataType dataType);

    static Format canonicalFormat(InternalFormat format) noexcept;

    static DataType canonicalDataType(InternalFormat format) noexcept;

    static Filter texelFilter(Filter filter) noexcept;

    static bool isTrilinear(Filter filter) noexcept;

    template <typename T>
    static T readComponent(const std::byte* source) noexcept {
        T value;
        std::memcpy(&value, source, sizeof(T));
        return value;
    }

    template <typename T>
    static void writeComponent(std::byte* destination, T value) noexcept {
        std::memcpy(destination, &value, sizeof(T));
    }
};
