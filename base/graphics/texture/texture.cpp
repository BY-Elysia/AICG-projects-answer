#include <iostream>
#include <sstream>

#include "graphics/texture/texture.h"
#include "graphics/texture/texture_utility.h"

namespace gfx {
Texture::Texture(Format format)
    : m_format{format} {}

Texture::Texture(Texture&& rhs) noexcept
    : RHIResource{std::move(rhs)}
    , m_format{std::move(rhs.m_format)}
    , m_devicePtr{std::exchange(rhs.m_devicePtr, 0ull)} {}

Texture::~Texture() {
    if (m_devicePtr) {
        glMakeTextureHandleNonResidentARB(m_devicePtr);
    }

    if (m_handle) {
        glDeleteTextures(1, &m_handle);
    }
}

Texture& Texture::operator=(Texture&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            if (m_devicePtr) {
                glMakeTextureHandleNonResidentARB(m_devicePtr);
            }

            glDeleteTextures(1, &m_handle);
        }

        m_devicePtr = std::exchange(rhs.m_devicePtr, 0ull);
        RHIResource::operator=(std::move(rhs));
    }

    return *this;
}

void Texture::bindTextureUnit(uint32_t texUnit) const {
    glBindTextureUnit(texUnit, m_handle);
}

void Texture::unbindTextureUnit(uint32_t texUnit) {
    glBindTextureUnit(texUnit, 0);
}

void Texture::clear(ExternalFormat format, PixelType type, void const* data, int level) {
    glClearTexImage(m_handle, level, details::toNativeExternalFormat(format), details::toNativeDataType(type), data);
}

void Texture::makeDeviceResident(Sampler const& sampler) {
    if (m_devicePtr) {
        std::cerr << "The texture is already located in device memory with all states baked\n";
        return;
    }

    // Once the device ptr is requested, it is assumed that the texture is immutable(bindless)
    // API calls such as glTexImage2D are not allowed
    m_devicePtr = glGetTextureSamplerHandleARB(m_handle, sampler.getNativeHandle());
    glMakeTextureHandleResidentARB(m_devicePtr);
}

void Texture::makeDeviceNonResident() {
    if (m_devicePtr) {
        glMakeTextureHandleNonResidentARB(m_devicePtr);
        m_devicePtr = 0ull;
    }
}

uint64_t Texture::getDevicePtr() const noexcept {
    return m_devicePtr;
}
} // namespace gfx
