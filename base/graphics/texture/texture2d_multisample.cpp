#include "graphics/texture/texture2d_multisample.h"

#include <stdexcept>

#include "graphics/gl_utility.h"
#include "graphics/texture/texture_utility.h"

namespace gfx {
Texture2DMultisample::Texture2DMultisample(Specification const& spec)
    : Texture{spec.format}
    , m_width{spec.width}
    , m_height{spec.height}
    , m_samples{spec.samples}
    , m_fixSampleLocations{spec.fixedSampleLocations}
    , m_immutable{spec.isImmutable} {
    if (m_width == 0) {
        throw std::runtime_error("Invalid texture width");
    }
    if (m_height == 0) {
        throw std::runtime_error("Invalid texture height");
    }

    GLint format{details::toNativeFormat(m_format)};
    if (m_immutable) {
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &m_handle);
        glTextureStorage2DMultisample(m_handle, m_samples, format, m_width, m_height, spec.fixedSampleLocations);
    }
    else {
        glGenTextures(1, &m_handle);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_handle);
        glTexImage2DMultisample(
            GL_TEXTURE_2D_MULTISAMPLE, m_samples, format, m_width, m_height, spec.fixedSampleLocations);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    }
}

void Texture2DMultisample::resize(uint32_t width, uint32_t height) {
    if (m_immutable) {
        throw std::runtime_error("Immutable texture cannot be resized");
    }

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_handle);
    glTexImage2DMultisample(
        GL_TEXTURE_2D_MULTISAMPLE, m_samples, details::toNativeFormat(m_format), width, height, m_fixSampleLocations);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    m_width = width;
    m_height = height;
}
} // namespace gfx
