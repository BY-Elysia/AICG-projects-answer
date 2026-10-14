#include "cpu_texture2d.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

float CpuTexture2D::halfToFloat(uint16_t bits) noexcept {
    float const sign = (bits & 0x8000u) == 0u ? 1.0f : -1.0f;
    uint16_t const exponent = (bits >> 10u) & 0x1fu;
    uint16_t const mantissa = bits & 0x03ffu;
    if (exponent == 0u) {
        return sign * std::ldexp(static_cast<float>(mantissa), -24);
    }
    if (exponent == 0x1fu) {
        if (mantissa == 0u) {
            return sign * std::numeric_limits<float>::infinity();
        }
        return std::numeric_limits<float>::quiet_NaN();
    }
    return sign * std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f, exponent - 15);
}

uint16_t CpuTexture2D::floatToHalf(float value) noexcept {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    uint16_t const sign = static_cast<uint16_t>((bits >> 16u) & 0x8000u);
    uint32_t const sourceExponent = (bits >> 23u) & 0xffu;
    uint32_t mantissa = bits & 0x007fffffu;
    if (sourceExponent == 0xffu) {
        if (mantissa == 0u) {
            return sign | 0x7c00u;
        }
        return sign | 0x7e00u;
    }

    int exponent = static_cast<int>(sourceExponent) - 127 + 15;
    if (exponent >= 31) {
        return sign | 0x7c00u;
    }
    if (exponent <= 0) {
        if (exponent < -10) {
            return sign;
        }
        mantissa |= 0x00800000u;
        uint32_t const shift = static_cast<uint32_t>(14 - exponent);
        uint32_t result = mantissa >> shift;
        if (((mantissa >> (shift - 1u)) & 1u) != 0u) {
            ++result;
        }
        return sign | static_cast<uint16_t>(result);
    }

    mantissa += 0x00001000u;
    if ((mantissa & 0x00800000u) != 0u) {
        mantissa = 0u;
        ++exponent;
        if (exponent >= 31) {
            return sign | 0x7c00u;
        }
    }
    return sign | static_cast<uint16_t>(exponent << 10u) | static_cast<uint16_t>(mantissa >> 13u);
}

CpuTexture2D::CpuTexture2D(
    InternalFormat internalFormat, int width, int height, Format format, DataType dataType, void const* pixels)
    : m_internalFormat(internalFormat)
    , m_format(format)
    , m_dataType(dataType)
    , m_components(componentCount(internalFormat))
    , m_bytesPerComponent(bytesPerComponent(internalFormat)) {
    if (width <= 0 || height <= 0) {
        throw std::logic_error("CpuTexture2D dimensions must be positive");
    }

    m_mipLevels.push_back({width, height, nullptr});
    size_t const storageSize = byteSize();
    m_mipLevels.front().pixels = ::operator new(storageSize);
    updatePixels(pixels);
}

CpuTexture2D::~CpuTexture2D() {
    clearMipLevels();
}

CpuTexture2D::CpuTexture2D(CpuTexture2D&& rhs) noexcept
    : m_internalFormat(rhs.m_internalFormat)
    , m_format(rhs.m_format)
    , m_dataType(rhs.m_dataType)
    , m_components(std::exchange(rhs.m_components, 0))
    , m_bytesPerComponent(std::exchange(rhs.m_bytesPerComponent, 0))
    , m_addressModeS(rhs.m_addressModeS)
    , m_addressModeT(rhs.m_addressModeT)
    , m_minFilter(rhs.m_minFilter)
    , m_magFilter(rhs.m_magFilter)
    , m_mipLevels(std::exchange(rhs.m_mipLevels, {})) {}

CpuTexture2D& CpuTexture2D::operator=(CpuTexture2D&& rhs) noexcept {
    if (this != &rhs) {
        clearMipLevels();
        m_internalFormat = rhs.m_internalFormat;
        m_format = rhs.m_format;
        m_dataType = rhs.m_dataType;
        m_components = std::exchange(rhs.m_components, 0);
        m_bytesPerComponent = std::exchange(rhs.m_bytesPerComponent, 0);
        m_addressModeS = rhs.m_addressModeS;
        m_addressModeT = rhs.m_addressModeT;
        m_minFilter = rhs.m_minFilter;
        m_magFilter = rhs.m_magFilter;
        m_mipLevels = std::exchange(rhs.m_mipLevels, {});
    }

    return *this;
}

void CpuTexture2D::setAddressModeS(AddressMode addressMode) {
    m_addressModeS = addressMode;
}

void CpuTexture2D::setAddressModeT(AddressMode addressMode) {
    m_addressModeT = addressMode;
}

void CpuTexture2D::setMinFilter(Filter filter) {
    m_minFilter = filter;
}

void CpuTexture2D::setMagFilter(Filter filter) {
    m_magFilter = filter;
}

void CpuTexture2D::generatorMipMap() {
    clearGeneratedMipLevels();
    int sourceWidth = m_mipLevels.front().width;
    int sourceHeight = m_mipLevels.front().height;
    void const* sourcePixels = m_mipLevels.front().pixels;
    while (sourceWidth > 1 || sourceHeight > 1) {
        int const destinationWidth = std::max(1, sourceWidth / 2);
        int const destinationHeight = std::max(1, sourceHeight / 2);
        void* destinationPixels = ::operator new(levelByteSize(destinationWidth, destinationHeight));
        size_t const destinationStride = m_components * m_bytesPerComponent;
        for (int y = 0; y < destinationHeight; ++y) {
            for (int x = 0; x < destinationWidth; ++x) {
                int const sourceX = x * 2;
                int const sourceY = y * 2;
                glm::vec4 const average =
                    0.25f * (texelFromLevel(sourcePixels, sourceWidth, sourceHeight, sourceX, sourceY) +
                             texelFromLevel(sourcePixels, sourceWidth, sourceHeight, sourceX + 1, sourceY) +
                             texelFromLevel(sourcePixels, sourceWidth, sourceHeight, sourceX, sourceY + 1) +
                             texelFromLevel(sourcePixels, sourceWidth, sourceHeight, sourceX + 1, sourceY + 1));
                auto* destination = static_cast<std::byte*>(destinationPixels) +
                                    (static_cast<size_t>(y) * destinationWidth + x) * destinationStride;
                writeTexel(destination, average);
            }
        }
        m_mipLevels.push_back({destinationWidth, destinationHeight, destinationPixels});
        sourceWidth = destinationWidth;
        sourceHeight = destinationHeight;
        sourcePixels = destinationPixels;
    }
}

void CpuTexture2D::updatePixels(void const* pixels) {
    if (!pixels) {
        return;
    }

    clearGeneratedMipLevels();

    size_t const sourceStride = sourceComponentCount(m_format) * sourceBytesPerComponent(m_dataType);
    MipLevel const& baseLevel = m_mipLevels.front();
    size_t const texelCount = static_cast<size_t>(baseLevel.width) * static_cast<size_t>(baseLevel.height);
    auto const* sourcePixels = static_cast<std::byte const*>(pixels);
    auto* destinationPixels = static_cast<std::byte*>(m_mipLevels.front().pixels);
    size_t const destinationStride = m_components * m_bytesPerComponent;
    for (size_t texelIndex = 0; texelIndex < texelCount; ++texelIndex) {
        glm::vec4 const value = sourceTexel(sourcePixels + texelIndex * sourceStride);
        writeTexel(destinationPixels + texelIndex * destinationStride, value);
    }
}

glm::vec4 CpuTexture2D::sample(glm::vec2 const& uv) const {
    return sampleLevel(uv, 0, texelFilter(m_minFilter));
}

glm::vec4 CpuTexture2D::sampleMipMap(glm::vec2 const& uv, float levelOfDetail) const {
    float const maxLevel = static_cast<float>(m_mipLevels.size() - 1);
    float const clampedLevel = std::clamp(levelOfDetail, 0.0f, maxLevel);
    if (m_minFilter == Filter::Nearest || m_minFilter == Filter::Linear) {
        return sampleLevel(uv, 0, m_minFilter);
    }
    if (!isTrilinear(m_minFilter)) {
        size_t const level = static_cast<size_t>(std::floor(clampedLevel + 0.5f));
        return sampleLevel(uv, level, texelFilter(m_minFilter));
    }
    size_t const lowerLevel = static_cast<size_t>(std::floor(clampedLevel));
    size_t const upperLevel = std::min(lowerLevel + 1, m_mipLevels.size() - 1);
    float const blend = clampedLevel - static_cast<float>(lowerLevel);
    glm::vec4 const lower = sampleLevel(uv, lowerLevel, texelFilter(m_minFilter));
    glm::vec4 const upper = sampleLevel(uv, upperLevel, texelFilter(m_minFilter));
    return glm::mix(lower, upper, blend);
}

int CpuTexture2D::width() const {
    return m_mipLevels.front().width;
}

int CpuTexture2D::height() const {
    return m_mipLevels.front().height;
}

CpuTexture2D::InternalFormat CpuTexture2D::internalFormat() const {
    return m_internalFormat;
}

CpuTexture2D::Format CpuTexture2D::format() const {
    return m_format;
}

CpuTexture2D::DataType CpuTexture2D::dataType() const {
    return m_dataType;
}

void* CpuTexture2D::pixels() {
    return m_mipLevels.front().pixels;
}

void const* CpuTexture2D::pixels() const {
    return m_mipLevels.front().pixels;
}

CpuTexture2D::Format CpuTexture2D::storageFormat() const {
    return canonicalFormat(m_internalFormat);
}

CpuTexture2D::DataType CpuTexture2D::storageDataType() const {
    return canonicalDataType(m_internalFormat);
}

size_t CpuTexture2D::byteSize() const {
    return levelByteSize(m_mipLevels.front().width, m_mipLevels.front().height);
}

size_t CpuTexture2D::levelByteSize(int width, int height) const {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * m_components * m_bytesPerComponent;
}

void CpuTexture2D::clearMipLevels() noexcept {
    for (MipLevel& level : m_mipLevels) {
        ::operator delete(level.pixels);
    }
    m_mipLevels.clear();
}

void CpuTexture2D::clearGeneratedMipLevels() noexcept {
    if (m_mipLevels.size() <= 1) {
        return;
    }
    for (auto level = m_mipLevels.begin() + 1; level != m_mipLevels.end(); ++level) {
        ::operator delete(level->pixels);
    }
    m_mipLevels.erase(m_mipLevels.begin() + 1, m_mipLevels.end());
}

size_t CpuTexture2D::componentCount(InternalFormat format) {
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::R16F:
    case InternalFormat::R32F:
        return 1;
    case InternalFormat::RG8:
    case InternalFormat::RG16F:
    case InternalFormat::RG32F:
        return 2;
    case InternalFormat::RGB8:
    case InternalFormat::RGB16F:
    case InternalFormat::RGB32F:
        return 3;
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA16F:
    case InternalFormat::RGBA32F:
        return 4;
    default:
        throw std::logic_error("unsupported CpuTexture2D internal format");
    }
}

size_t CpuTexture2D::sourceComponentCount(Format format) {
    switch (format) {
    case Format::Red:
    case Format::Depth:
        return 1;
    case Format::RedGreen:
        return 2;
    case Format::RedGreenBlue:
    case Format::BlueGreenRed:
        return 3;
    case Format::RedGreenBlueAlpha:
    case Format::BlueGreenRedAlpha:
        return 4;
    default:
        throw std::logic_error("unsupported CpuTexture2D source format");
    }
}

size_t CpuTexture2D::bytesPerComponent(InternalFormat format) {
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::RG8:
    case InternalFormat::RGB8:
    case InternalFormat::RGBA8:
        return sizeof(uint8_t);
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F:
        return sizeof(uint16_t);
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F:
        return sizeof(float);
    default:
        throw std::logic_error("unsupported CpuTexture2D internal format");
    }
}

size_t CpuTexture2D::sourceBytesPerComponent(DataType dataType) {
    switch (dataType) {
    case DataType::UnsignedByte:
    case DataType::Byte:
        return sizeof(uint8_t);
    case DataType::UnsignedShort:
    case DataType::Short:
    case DataType::HalfFloat:
        return sizeof(uint16_t);
    case DataType::UnsignedInt:
    case DataType::Int:
    case DataType::Float:
        return sizeof(uint32_t);
    default:
        throw std::logic_error("unsupported CpuTexture2D source data type");
    }
}

CpuTexture2D::Format CpuTexture2D::canonicalFormat(InternalFormat format) noexcept {
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::R16F:
    case InternalFormat::R32F:
        return Format::Red;
    case InternalFormat::RG8:
    case InternalFormat::RG16F:
    case InternalFormat::RG32F:
        return Format::RedGreen;
    case InternalFormat::RGB8:
    case InternalFormat::RGB16F:
    case InternalFormat::RGB32F:
        return Format::RedGreenBlue;
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA16F:
    case InternalFormat::RGBA32F:
        return Format::RedGreenBlueAlpha;
    }
    return Format::RedGreenBlueAlpha;
}

CpuTexture2D::DataType CpuTexture2D::canonicalDataType(InternalFormat format) noexcept {
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::RG8:
    case InternalFormat::RGB8:
    case InternalFormat::RGBA8:
        return DataType::UnsignedByte;
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F:
        return DataType::HalfFloat;
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F:
        return DataType::Float;
    }
    return DataType::UnsignedByte;
}

CpuTexture2D::Filter CpuTexture2D::texelFilter(Filter filter) noexcept {
    switch (filter) {
    case Filter::Nearest:
    case Filter::NearestMipmapNearest:
    case Filter::NearestMipmapLinear:
        return Filter::Nearest;
    case Filter::Linear:
    case Filter::LinearMipmapNearest:
    case Filter::LinearMipmapLinear:
        return Filter::Linear;
    }
    return Filter::Nearest;
}

bool CpuTexture2D::isTrilinear(Filter filter) noexcept {
    switch (filter) {
    case Filter::NearestMipmapLinear:
    case Filter::LinearMipmapLinear:
        return true;
    case Filter::Nearest:
    case Filter::Linear:
    case Filter::NearestMipmapNearest:
    case Filter::LinearMipmapNearest:
        return false;
    }
    return false;
}

glm::vec4 CpuTexture2D::texelFromLevel(void const* pixels, int width, int height, int x, int y) const {
    x = resolveCoordinate(x, width, m_addressModeS);
    y = resolveCoordinate(y, height, m_addressModeT);
    size_t const texelIndex = static_cast<size_t>(y) * width + x;
    size_t const texelStride = m_components * m_bytesPerComponent;
    auto const* source = static_cast<std::byte const*>(pixels) + texelIndex * texelStride;

    glm::vec4 result(0.0f, 0.0f, 0.0f, 1.0f);
    for (size_t index = 0; index < m_components; ++index) {
        result[static_cast<glm::length_t>(index)] = storedComponent(source + index * m_bytesPerComponent);
    }

    return result;
}

glm::vec4 CpuTexture2D::sampleLevel(glm::vec2 const& inputUv, size_t level, Filter filter) const {
    MipLevel const& mipLevel = m_mipLevels[std::min(level, m_mipLevels.size() - 1)];
    void const* levelPixels = mipLevel.pixels;
    int const levelWidth = mipLevel.width;
    int const levelHeight = mipLevel.height;
    glm::vec2 const uv(resolveUv(inputUv.x, m_addressModeS), resolveUv(inputUv.y, m_addressModeT));
    if (filter == Filter::Nearest) {
        return texelFromLevel(levelPixels,
                              levelWidth,
                              levelHeight,
                              static_cast<int>(std::floor(uv.x * static_cast<float>(levelWidth))),
                              static_cast<int>(std::floor(uv.y * static_cast<float>(levelHeight))));
    }

    float const x = uv.x * static_cast<float>(levelWidth) - 0.5f;
    float const y = uv.y * static_cast<float>(levelHeight) - 0.5f;
    int const x0 = static_cast<int>(std::floor(x));
    int const y0 = static_cast<int>(std::floor(y));
    float const tx = x - std::floor(x);
    float const ty = y - std::floor(y);
    glm::vec4 const lower = glm::mix(texelFromLevel(levelPixels, levelWidth, levelHeight, x0, y0),
                                     texelFromLevel(levelPixels, levelWidth, levelHeight, x0 + 1, y0),
                                     tx);
    glm::vec4 const upper = glm::mix(texelFromLevel(levelPixels, levelWidth, levelHeight, x0, y0 + 1),
                                     texelFromLevel(levelPixels, levelWidth, levelHeight, x0 + 1, y0 + 1),
                                     tx);

    return glm::mix(lower, upper, ty);
}

glm::vec4 CpuTexture2D::sourceTexel(std::byte const* source) const {
    size_t const sourceComponents = sourceComponentCount(m_format);
    size_t const sourceBytes = sourceBytesPerComponent(m_dataType);
    glm::vec4 result(0.0f, 0.0f, 0.0f, 1.0f);
    for (size_t index = 0; index < sourceComponents; ++index) {
        result[static_cast<glm::length_t>(index)] = sourceComponent(source + index * sourceBytes);
    }

    if (m_format == Format::BlueGreenRed || m_format == Format::BlueGreenRedAlpha) {
        return glm::vec4(result.b, result.g, result.r, result.a);
    }

    return result;
}

void CpuTexture2D::writeTexel(std::byte* destination, glm::vec4 const& value) const {
    for (size_t index = 0; index < m_components; ++index) {
        float const componentValue = value[static_cast<glm::length_t>(index)];
        std::byte* componentDestination = destination + index * m_bytesPerComponent;
        switch (m_internalFormat) {
        case InternalFormat::R8:
        case InternalFormat::RG8:
        case InternalFormat::RGB8:
        case InternalFormat::RGBA8: {
            float const normalized = std::clamp(componentValue, 0.0f, 1.0f);
            auto const encoded = static_cast<uint8_t>(std::round(normalized * 255.0f));
            writeComponent(componentDestination, encoded);
            break;
        }
        case InternalFormat::R16F:
        case InternalFormat::RG16F:
        case InternalFormat::RGB16F:
        case InternalFormat::RGBA16F:
            writeComponent(componentDestination, floatToHalf(componentValue));
            break;
        case InternalFormat::R32F:
        case InternalFormat::RG32F:
        case InternalFormat::RGB32F:
        case InternalFormat::RGBA32F:
            writeComponent(componentDestination, componentValue);
            break;
        }
    }
}

float CpuTexture2D::sourceComponent(std::byte const* source) const {
    switch (m_dataType) {
    case DataType::UnsignedByte:
        return static_cast<float>(readComponent<uint8_t>(source)) /
               static_cast<float>(std::numeric_limits<uint8_t>::max());
    case DataType::Byte:
        return std::max(static_cast<float>(readComponent<int8_t>(source)) /
                            static_cast<float>(std::numeric_limits<int8_t>::max()),
                        -1.0f);
    case DataType::UnsignedShort:
        return static_cast<float>(readComponent<uint16_t>(source)) /
               static_cast<float>(std::numeric_limits<uint16_t>::max());
    case DataType::Short:
        return std::max(static_cast<float>(readComponent<int16_t>(source)) /
                            static_cast<float>(std::numeric_limits<int16_t>::max()),
                        -1.0f);
    case DataType::HalfFloat:
        return halfToFloat(readComponent<uint16_t>(source));
    case DataType::UnsignedInt:
        return static_cast<float>(readComponent<uint32_t>(source)) /
               static_cast<float>(std::numeric_limits<uint32_t>::max());
    case DataType::Int:
        return std::max(static_cast<float>(readComponent<int32_t>(source)) /
                            static_cast<float>(std::numeric_limits<int32_t>::max()),
                        -1.0f);
    case DataType::Float:
        return readComponent<float>(source);
    }
    return 0.0f;
}

float CpuTexture2D::storedComponent(std::byte const* source) const {
    switch (m_internalFormat) {
    case InternalFormat::R8:
    case InternalFormat::RG8:
    case InternalFormat::RGB8:
    case InternalFormat::RGBA8:
        return static_cast<float>(readComponent<uint8_t>(source)) /
               static_cast<float>(std::numeric_limits<uint8_t>::max());
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F:
        return halfToFloat(readComponent<uint16_t>(source));
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F:
        return readComponent<float>(source);
    }

    return 0.0f;
}

int CpuTexture2D::resolveCoordinate(int coordinate, int extent, AddressMode addressMode) noexcept {
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

float CpuTexture2D::resolveUv(float coordinate, AddressMode addressMode) noexcept {
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
