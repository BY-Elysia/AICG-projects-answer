#include "cpu_texture2d.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

float CpuTexture2D::halfToFloat(uint16_t bits) noexcept {
    const float sign = (bits & 0x8000u) == 0u ? 1.0f : -1.0f;
    const uint16_t exponent = (bits >> 10u) & 0x1fu;
    const uint16_t mantissa = bits & 0x03ffu;
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
    const uint16_t sign = static_cast<uint16_t>((bits >> 16u) & 0x8000u);
    const uint32_t sourceExponent = (bits >> 23u) & 0xffu;
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
        const uint32_t shift = static_cast<uint32_t>(14 - exponent);
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
    return sign | static_cast<uint16_t>(exponent << 10u)
           | static_cast<uint16_t>(mantissa >> 13u);
}

CpuTexture2D::CpuTexture2D(
    InternalFormat internalFormat, int width, int height, Format format, DataType dataType,
    const void* pixels)
    : m_internalFormat(internalFormat), m_format(format), m_dataType(dataType),
      m_components(componentCount(internalFormat)),
      m_bytesPerComponent(bytesPerComponent(internalFormat)) {
    if (width <= 0 || height <= 0) {
        throw std::logic_error("CpuTexture2D dimensions must be positive");
    }

    m_mipLevels.push_back({width, height, nullptr});
    const size_t storageSize = byteSize();
    m_mipLevels.front().pixels = ::operator new(storageSize);
    updatePixels(pixels);
}

CpuTexture2D::~CpuTexture2D() {
    clearMipLevels();
}

CpuTexture2D::CpuTexture2D(CpuTexture2D&& rhs) noexcept
    : m_internalFormat(rhs.m_internalFormat), m_format(rhs.m_format), m_dataType(rhs.m_dataType),
      m_components(std::exchange(rhs.m_components, 0)),
      m_bytesPerComponent(std::exchange(rhs.m_bytesPerComponent, 0)),
      m_addressModeS(rhs.m_addressModeS), m_addressModeT(rhs.m_addressModeT),
      m_minFilter(rhs.m_minFilter), m_magFilter(rhs.m_magFilter),
      m_mipLevels(std::exchange(rhs.m_mipLevels, {})) {}

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
    const void* sourcePixels = m_mipLevels.front().pixels;
    while (sourceWidth > 1 || sourceHeight > 1) {
        const int destinationWidth = std::max(1, sourceWidth / 2);
        const int destinationHeight = std::max(1, sourceHeight / 2);
        void* destinationPixels = ::operator new(levelByteSize(destinationWidth, destinationHeight));
        const size_t destinationStride = m_components * m_bytesPerComponent;
        for (int y = 0; y < destinationHeight; ++y) {
            for (int x = 0; x < destinationWidth; ++x) {
                const int sourceX = x * 2;
                const int sourceY = y * 2;
                const glm::vec4 average = 0.25f
                                          * (texelFromLevel(
                                                 sourcePixels, sourceWidth, sourceHeight, sourceX,
                                                 sourceY)
                                             + texelFromLevel(
                                                 sourcePixels, sourceWidth, sourceHeight,
                                                 sourceX + 1, sourceY)
                                             + texelFromLevel(
                                                 sourcePixels, sourceWidth, sourceHeight, sourceX,
                                                 sourceY + 1)
                                             + texelFromLevel(
                                                 sourcePixels, sourceWidth, sourceHeight,
                                                 sourceX + 1, sourceY + 1));
                auto* destination = static_cast<std::byte*>(destinationPixels)
                                    + (static_cast<size_t>(y) * destinationWidth + x)
                                          * destinationStride;
                writeTexel(destination, average);
            }
        }
        m_mipLevels.push_back({destinationWidth, destinationHeight, destinationPixels});
        sourceWidth = destinationWidth;
        sourceHeight = destinationHeight;
        sourcePixels = destinationPixels;
    }
}

void CpuTexture2D::updatePixels(const void* pixels) {
    if (!pixels) {
        return;
    }

    clearGeneratedMipLevels();

    const size_t sourceStride =
        sourceComponentCount(m_format) * sourceBytesPerComponent(m_dataType);
    const MipLevel& baseLevel = m_mipLevels.front();
    const size_t texelCount =
        static_cast<size_t>(baseLevel.width) * static_cast<size_t>(baseLevel.height);
    const auto* sourcePixels = static_cast<const std::byte*>(pixels);
    auto* destinationPixels = static_cast<std::byte*>(m_mipLevels.front().pixels);
    const size_t destinationStride = m_components * m_bytesPerComponent;
    for (size_t texelIndex = 0; texelIndex < texelCount; ++texelIndex) {
        const glm::vec4 value = sourceTexel(sourcePixels + texelIndex * sourceStride);
        writeTexel(destinationPixels + texelIndex * destinationStride, value);
    }
}

glm::vec4 CpuTexture2D::sample(const glm::vec2& uv) const {
    return sampleLevel(uv, 0, texelFilter(m_minFilter));
}

glm::vec4 CpuTexture2D::sampleMipMap(const glm::vec2& uv, float levelOfDetail) const {
    const float maxLevel = static_cast<float>(m_mipLevels.size() - 1);
    const float clampedLevel = std::clamp(levelOfDetail, 0.0f, maxLevel);
    if (m_minFilter == Filter::Nearest || m_minFilter == Filter::Linear) {
        return sampleLevel(uv, 0, m_minFilter);
    }
    if (!isTrilinear(m_minFilter)) {
        const size_t level = static_cast<size_t>(std::floor(clampedLevel + 0.5f));
        return sampleLevel(uv, level, texelFilter(m_minFilter));
    }
    const size_t lowerLevel = static_cast<size_t>(std::floor(clampedLevel));
    const size_t upperLevel = std::min(lowerLevel + 1, m_mipLevels.size() - 1);
    const float blend = clampedLevel - static_cast<float>(lowerLevel);
    const glm::vec4 lower = sampleLevel(uv, lowerLevel, texelFilter(m_minFilter));
    const glm::vec4 upper = sampleLevel(uv, upperLevel, texelFilter(m_minFilter));
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

const void* CpuTexture2D::pixels() const {
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
    return static_cast<size_t>(width) * static_cast<size_t>(height) * m_components
           * m_bytesPerComponent;
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
    case InternalFormat::R32F: return 1;
    case InternalFormat::RG8:
    case InternalFormat::RG16F:
    case InternalFormat::RG32F: return 2;
    case InternalFormat::RGB8:
    case InternalFormat::RGB16F:
    case InternalFormat::RGB32F: return 3;
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA16F:
    case InternalFormat::RGBA32F: return 4;
    default: throw std::logic_error("unsupported CpuTexture2D internal format");
    }
}

size_t CpuTexture2D::sourceComponentCount(Format format) {
    switch (format) {
    case Format::Red:
    case Format::Depth: return 1;
    case Format::RedGreen: return 2;
    case Format::RedGreenBlue:
    case Format::BlueGreenRed: return 3;
    case Format::RedGreenBlueAlpha:
    case Format::BlueGreenRedAlpha: return 4;
    default: throw std::logic_error("unsupported CpuTexture2D source format");
    }
}

size_t CpuTexture2D::bytesPerComponent(InternalFormat format) {
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::RG8:
    case InternalFormat::RGB8:
    case InternalFormat::RGBA8: return sizeof(uint8_t);
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F: return sizeof(uint16_t);
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F: return sizeof(float);
    default: throw std::logic_error("unsupported CpuTexture2D internal format");
    }
}

size_t CpuTexture2D::sourceBytesPerComponent(DataType dataType) {
    switch (dataType) {
    case DataType::UnsignedByte:
    case DataType::Byte: return sizeof(uint8_t);
    case DataType::UnsignedShort:
    case DataType::Short:
    case DataType::HalfFloat: return sizeof(uint16_t);
    case DataType::UnsignedInt:
    case DataType::Int:
    case DataType::Float: return sizeof(uint32_t);
    default: throw std::logic_error("unsupported CpuTexture2D source data type");
    }
}

CpuTexture2D::Format CpuTexture2D::canonicalFormat(InternalFormat format) noexcept {
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::R16F:
    case InternalFormat::R32F: return Format::Red;
    case InternalFormat::RG8:
    case InternalFormat::RG16F:
    case InternalFormat::RG32F: return Format::RedGreen;
    case InternalFormat::RGB8:
    case InternalFormat::RGB16F:
    case InternalFormat::RGB32F: return Format::RedGreenBlue;
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA16F:
    case InternalFormat::RGBA32F: return Format::RedGreenBlueAlpha;
    }
    return Format::RedGreenBlueAlpha;
}

CpuTexture2D::DataType CpuTexture2D::canonicalDataType(InternalFormat format) noexcept {
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::RG8:
    case InternalFormat::RGB8:
    case InternalFormat::RGBA8: return DataType::UnsignedByte;
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F: return DataType::HalfFloat;
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F: return DataType::Float;
    }
    return DataType::UnsignedByte;
}

CpuTexture2D::Filter CpuTexture2D::texelFilter(Filter filter) noexcept {
    switch (filter) {
    case Filter::Nearest:
    case Filter::NearestMipmapNearest:
    case Filter::NearestMipmapLinear: return Filter::Nearest;
    case Filter::Linear:
    case Filter::LinearMipmapNearest:
    case Filter::LinearMipmapLinear: return Filter::Linear;
    }
    return Filter::Nearest;
}

bool CpuTexture2D::isTrilinear(Filter filter) noexcept {
    switch (filter) {
    case Filter::NearestMipmapLinear:
    case Filter::LinearMipmapLinear: return true;
    case Filter::Nearest:
    case Filter::Linear:
    case Filter::NearestMipmapNearest:
    case Filter::LinearMipmapNearest: return false;
    }
    return false;
}

glm::vec4 CpuTexture2D::texelFromLevel(
    const void* pixels, int width, int height, int x, int y) const {
    x = resolveCoordinate(x, width, m_addressModeS);
    y = resolveCoordinate(y, height, m_addressModeT);
    const size_t texelIndex = static_cast<size_t>(y) * width + x;
    const size_t texelStride = m_components * m_bytesPerComponent;
    const auto* source = static_cast<const std::byte*>(pixels) + texelIndex * texelStride;

    glm::vec4 result(0.0f, 0.0f, 0.0f, 1.0f);
    for (size_t index = 0; index < m_components; ++index) {
        result[static_cast<glm::length_t>(index)] =
            storedComponent(source + index * m_bytesPerComponent);
    }

    return result;
}

glm::vec4 CpuTexture2D::sampleLevel(const glm::vec2& inputUv, size_t level, Filter filter) const {
    const MipLevel& mipLevel = m_mipLevels[std::min(level, m_mipLevels.size() - 1)];
    const void* levelPixels = mipLevel.pixels;
    const int levelWidth = mipLevel.width;
    const int levelHeight = mipLevel.height;
    const glm::vec2 uv(
        resolveUv(inputUv.x, m_addressModeS), resolveUv(inputUv.y, m_addressModeT));
    if (filter == Filter::Nearest) {
        return texelFromLevel(
            levelPixels, levelWidth, levelHeight,
            static_cast<int>(std::floor(uv.x * static_cast<float>(levelWidth))),
            static_cast<int>(std::floor(uv.y * static_cast<float>(levelHeight))));
    }

    const float x = uv.x * static_cast<float>(levelWidth) - 0.5f;
    const float y = uv.y * static_cast<float>(levelHeight) - 0.5f;
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = x - std::floor(x);
    const float ty = y - std::floor(y);
    const glm::vec4 lower = glm::mix(
        texelFromLevel(levelPixels, levelWidth, levelHeight, x0, y0),
        texelFromLevel(levelPixels, levelWidth, levelHeight, x0 + 1, y0), tx);
    const glm::vec4 upper = glm::mix(
        texelFromLevel(levelPixels, levelWidth, levelHeight, x0, y0 + 1),
        texelFromLevel(levelPixels, levelWidth, levelHeight, x0 + 1, y0 + 1), tx);

    return glm::mix(lower, upper, ty);
}

glm::vec4 CpuTexture2D::sourceTexel(const std::byte* source) const {
    const size_t sourceComponents = sourceComponentCount(m_format);
    const size_t sourceBytes = sourceBytesPerComponent(m_dataType);
    glm::vec4 result(0.0f, 0.0f, 0.0f, 1.0f);
    for (size_t index = 0; index < sourceComponents; ++index) {
        result[static_cast<glm::length_t>(index)] =
            sourceComponent(source + index * sourceBytes);
    }

    if (m_format == Format::BlueGreenRed || m_format == Format::BlueGreenRedAlpha) {
        return glm::vec4(result.b, result.g, result.r, result.a);
    }

    return result;
}

void CpuTexture2D::writeTexel(std::byte* destination, const glm::vec4& value) const {
    for (size_t index = 0; index < m_components; ++index) {
        const float componentValue = value[static_cast<glm::length_t>(index)];
        std::byte* componentDestination = destination + index * m_bytesPerComponent;
        switch (m_internalFormat) {
        case InternalFormat::R8:
        case InternalFormat::RG8:
        case InternalFormat::RGB8:
        case InternalFormat::RGBA8: {
            const float normalized = std::clamp(componentValue, 0.0f, 1.0f);
            const auto encoded = static_cast<uint8_t>(std::round(normalized * 255.0f));
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

float CpuTexture2D::sourceComponent(const std::byte* source) const {
    switch (m_dataType) {
    case DataType::UnsignedByte:
        return static_cast<float>(readComponent<uint8_t>(source))
               / static_cast<float>(std::numeric_limits<uint8_t>::max());
    case DataType::Byte:
        return std::max(
            static_cast<float>(readComponent<int8_t>(source))
                / static_cast<float>(std::numeric_limits<int8_t>::max()),
            -1.0f);
    case DataType::UnsignedShort:
        return static_cast<float>(readComponent<uint16_t>(source))
               / static_cast<float>(std::numeric_limits<uint16_t>::max());
    case DataType::Short:
        return std::max(
            static_cast<float>(readComponent<int16_t>(source))
                / static_cast<float>(std::numeric_limits<int16_t>::max()),
            -1.0f);
    case DataType::HalfFloat: return halfToFloat(readComponent<uint16_t>(source));
    case DataType::UnsignedInt:
        return static_cast<float>(readComponent<uint32_t>(source))
               / static_cast<float>(std::numeric_limits<uint32_t>::max());
    case DataType::Int:
        return std::max(
            static_cast<float>(readComponent<int32_t>(source))
                / static_cast<float>(std::numeric_limits<int32_t>::max()),
            -1.0f);
    case DataType::Float: return readComponent<float>(source);
    }
    return 0.0f;
}

float CpuTexture2D::storedComponent(const std::byte* source) const {
    switch (m_internalFormat) {
    case InternalFormat::R8:
    case InternalFormat::RG8:
    case InternalFormat::RGB8:
    case InternalFormat::RGBA8:
        return static_cast<float>(readComponent<uint8_t>(source))
               / static_cast<float>(std::numeric_limits<uint8_t>::max());
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F: return halfToFloat(readComponent<uint16_t>(source));
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F: return readComponent<float>(source);
    }

    return 0.0f;
}

int CpuTexture2D::resolveCoordinate(
    int coordinate, int extent, AddressMode addressMode) noexcept {
    switch (addressMode) {
    case AddressMode::Repeat: return (coordinate % extent + extent) % extent;
    case AddressMode::MirroredRepeat: {
        const int mirroredExtent = extent * 2;
        const int wrapped = (coordinate % mirroredExtent + mirroredExtent) % mirroredExtent;
        return wrapped < extent ? wrapped : mirroredExtent - 1 - wrapped;
    }
    case AddressMode::ClampToEdge: return std::clamp(coordinate, 0, extent - 1);
    }
    return std::clamp(coordinate, 0, extent - 1);
}

float CpuTexture2D::resolveUv(float coordinate, AddressMode addressMode) noexcept {
    switch (addressMode) {
    case AddressMode::Repeat: return glm::fract(coordinate);
    case AddressMode::MirroredRepeat: {
        const float interval = std::floor(coordinate);
        const float fraction = coordinate - interval;
        return std::fmod(interval, 2.0f) == 0.0f ? fraction : 1.0f - fraction;
    }
    case AddressMode::ClampToEdge: return std::clamp(coordinate, 0.0f, 1.0f);
    }

    return std::clamp(coordinate, 0.0f, 1.0f);
}
