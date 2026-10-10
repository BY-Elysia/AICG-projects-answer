#include "graphics/texture/texture2d.h"

#include <stdexcept>

#include "graphics/gl_utility.h"
#include "graphics/texture/texture_utility.h"

namespace gfx {
Texture2D::Texture2D(Specification const& spec, void const* data)
    : Texture{spec.format}
    , m_width{spec.width}
    , m_height{spec.height}
    , m_mipmapLevels{1}
    , m_immutable{spec.isImmutable} {
    if (m_width == 0) {
        throw std::runtime_error("Invalid texture width");
    }
    if (m_height == 0) {
        throw std::runtime_error("Invalid texture height");
    }

    if (m_format == Texture::Format::None) {
        m_format = details::chooseCompatibleFormat(spec.pixelFormat, spec.pixelType);
    }

    auto const maxMipmapLevel{details::getMaxMipmapLevels(m_width, m_height)};
    if (spec.generateMipmap) {
        if (spec.mipmapLevels == 0) {
            m_mipmapLevels = maxMipmapLevel;
        }
        else {
            m_mipmapLevels = std::min(maxMipmapLevel, spec.mipmapLevels);
        }
    }

    GLint format{details::toNativeFormat(m_format)};
    GLenum pixelFormat{details::toNativeExternalFormat(spec.pixelFormat)};
    GLenum dataType{details::toNativeDataType(spec.pixelType)};

    // pre allocate storage
    if (m_immutable) {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_handle);
        glTextureStorage2D(m_handle, static_cast<GLint>(m_mipmapLevels), format, m_width, m_height);
    }
    else {
        glGenTextures(1, &m_handle);
        glBindTexture(GL_TEXTURE_2D, m_handle);
        glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, pixelFormat, dataType, data);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    checkGLErrors();

    // transfer data
    update(data, spec.pixelFormat, spec.pixelType, 0);
    checkGLErrors();

    // generate mipmaps
    if (m_mipmapLevels > 1) {
        if (m_immutable) {
            glGenerateTextureMipmap(m_handle);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, m_handle);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    checkGLErrors();
}

uint32_t Texture2D::getNativeType() const {
    return GL_TEXTURE_2D;
}

void Texture2D::generateMipmap() {
    if (m_mipmapLevels == 1) {
        if (m_immutable) {
            throw std::runtime_error("Generating mipmap for immutable texture needs pre-allocated memory");
        }
        m_mipmapLevels = details::getMaxMipmapLevels(m_width, m_height);
    }

    if (m_immutable) {
        glGenerateTextureMipmap(m_handle);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, m_handle);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Texture2D::resize(uint32_t width, uint32_t height) {
    if (m_immutable) {
        throw std::runtime_error("Immutable texture cannot be resized");
    }
    if (m_devicePtr) {
        throw std::runtime_error("Device resident texture cannot be resized");
    }

    GLint format{details::toNativeFormat(m_format)};
    GLenum pixelFormat{details::toNativeExternalFormat(details::chooseCompatibleExternalFormat(m_format))};
    GLenum dataType{details::toNativeDataType(details::chooseCompatiblePixelType(m_format))};

    glBindTexture(GL_TEXTURE_2D, m_handle);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, pixelFormat, dataType, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_width = width;
    m_height = height;
    m_mipmapLevels = 1;
}

void Texture2D::update(void const* data, ExternalFormat pixelFormat, PixelType type, uint32_t level) {
    if (data == nullptr) {
        return;
    }

    GLenum externalFormat{details::toNativeExternalFormat(pixelFormat)};
    GLenum dataType{details::toNativeDataType(type)};

    GLint alignment{1};
    size_t pixelSize{details::getPixelSize(pixelFormat, type)};
    size_t pitch = m_width * pixelSize;
    if (pitch % 8 == 0) {
        alignment = 8;
    }
    else if (pitch % 4 == 0) {
        alignment = 4;
    }
    else if (pitch % 2 == 0) {
        alignment = 2;
    }

    GLint unpackAlignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);

    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

    uint32_t width{m_width};
    uint32_t height{m_height};
    for (uint32_t i = 0; i < level; ++i) {
        if (width == 1 && height == 1) {
            break;
        }

        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }

    if (m_immutable) {
        glTextureSubImage2D(m_handle, level, 0, 0, width, height, externalFormat, dataType, data);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, m_handle);
        glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, externalFormat, dataType, data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // 3. restore alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
}

void Texture2D::bindImage(uint32_t unit, Access access, int level) {
    GLenum const imageFormat{details::getImageFormat(m_format)};
    GLenum const imageAccess{details::getImageAccess(access)};
    glBindImageTexture(unit, m_handle, level, GL_FALSE, 0, imageAccess, imageFormat);
}

void Texture2D::unbindImage(uint32_t unit) {
    glBindImageTexture(unit, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
}

void Texture2D::copyFromFramebuffer(int x, int y, int width, int height, int level) {
    // TODO: internal format is much limited
    // @ref https://registry.khronos.org/OpenGL-Refpages/gl4/html/glCopyTexImage2D.xhtml
    // GLint Format{ details::getGraphicsAPIFormat(m_specification.Format) };
    // glCopyTexImage2D(GL_TEXTURE_2D, level, Format, x, y, width, height, 0);

    // TODO: Check it...
    // glCopyTextureImage2DEXT(m_handle, GL_TEXTURE_2D, level, Format, x, y, width, height, 0);
    glCopyTextureSubImage2D(m_handle, level, 0, 0, x, y, width, height);
}
} // namespace gfx
