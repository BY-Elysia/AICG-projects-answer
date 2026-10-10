#include "graphics/sampler.h"

#include <stdexcept>

#include "graphics/gl_utility.h"

namespace gfx {
namespace details {
static GLint toNativeAddressMode(Sampler::AddressMode mode) {
    switch (mode) {
    case Sampler::AddressMode::Repeat:
        return GL_REPEAT;
    case Sampler::AddressMode::ClampToEdge:
        return GL_CLAMP_TO_EDGE;
    case Sampler::AddressMode::ClampToBorder:
        return GL_CLAMP_TO_BORDER;
    case Sampler::AddressMode::MirroredRepeat:
        return GL_MIRRORED_REPEAT;
    }

    throw std::runtime_error("Invalid sampler address mode");
}

static GLint toNativeFilter(Sampler::Filter filter) {
    return filter == Sampler::Filter::Linear ? GL_LINEAR : GL_NEAREST;
}

static GLint toNativeFilter(Sampler::Filter filter, Sampler::MipmapMode mode) {
    if (filter == Sampler::Filter::Nearest) {
        if (mode == Sampler::MipmapMode::None) {
            return GL_NEAREST;
        }
        else if (mode == Sampler::MipmapMode::Nearest) {
            return GL_NEAREST_MIPMAP_NEAREST;
        }
        else if (mode == Sampler::MipmapMode::Linear) {
            return GL_NEAREST_MIPMAP_LINEAR;
        }
    }
    else if (filter == Sampler::Filter::Linear) {
        if (mode == Sampler::MipmapMode::None) {
            return GL_LINEAR;
        }
        else if (mode == Sampler::MipmapMode::Nearest) {
            return GL_LINEAR_MIPMAP_NEAREST;
        }
        else if (mode == Sampler::MipmapMode::Linear) {
            return GL_LINEAR_MIPMAP_LINEAR;
        }
    }

    throw std::runtime_error("Invalid sampler filter or mipmap mode");
}

static GLint toNativeCompareOp(Sampler::CompareOp op) {
    switch (op) {
    case Sampler::CompareOp::Never:
        return GL_NEVER;
    case Sampler::CompareOp::Less:
        return GL_LESS;
    case Sampler::CompareOp::Equal:
        return GL_EQUAL;
    case Sampler::CompareOp::LessEqual:
        return GL_LEQUAL;
    case Sampler::CompareOp::Greater:
        return GL_GREATER;
    case Sampler::CompareOp::NotEqual:
        return GL_NOTEQUAL;
    case Sampler::CompareOp::GreaterEqual:
        return GL_GEQUAL;
    case Sampler::CompareOp::Always:
        return GL_ALWAYS;
    }

    throw std::runtime_error("Invalid sampler compare operation");
}
} // namespace details

Sampler::Sampler()
    : Sampler(Description{}) {}

Sampler::Sampler(Description createInfo)
    : m_description{createInfo} {
    glGenSamplers(1, &m_handle);

    glSamplerParameteri(
        m_handle, GL_TEXTURE_MIN_FILTER, details::toNativeFilter(createInfo.minFilter, createInfo.mipmapMode));
    glSamplerParameteri(m_handle, GL_TEXTURE_MAG_FILTER, details::toNativeFilter(createInfo.magFilter));

    glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_S, details::toNativeAddressMode(createInfo.addressModeU));
    glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_T, details::toNativeAddressMode(createInfo.addressModeV));
    glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_R, details::toNativeAddressMode(createInfo.addressModeW));

    glSamplerParameterfv(m_handle, GL_TEXTURE_BORDER_COLOR, &createInfo.borderColor[0]);

    glSamplerParameterf(m_handle, GL_TEXTURE_LOD_BIAS, createInfo.mipLodBias);
    glSamplerParameterf(m_handle, GL_TEXTURE_MIN_LOD, createInfo.minLod);
    glSamplerParameterf(m_handle, GL_TEXTURE_MAX_LOD, createInfo.maxLod);

    if (createInfo.compareEnable) {
        glSamplerParameteri(m_handle, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(m_handle, GL_TEXTURE_COMPARE_FUNC, details::toNativeCompareOp(createInfo.compareOp));
    }
    else {
        glSamplerParameteri(m_handle, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    }

    if (createInfo.aniostropyEnable) {
        glSamplerParameterf(m_handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, createInfo.maxAnisotropy);
    }
}

Sampler::Sampler(Sampler&& rhs) noexcept
    : RHIResource{std::move(rhs)}
    , m_description{std::move(rhs.m_description)} {}

Sampler::~Sampler() {
    if (isValid()) {
        glDeleteSamplers(1, &m_handle);
    }
}

Sampler& Sampler::operator=(Sampler&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            glDeleteSamplers(1, &m_handle);
        }

        m_description = std::move(rhs.m_description);
        RHIResource::operator=(std::move(rhs));
    }

    return *this;
}

void Sampler::bind(uint32_t texUnit) const {
    glBindSampler(static_cast<GLuint>(texUnit), m_handle);
}

void Sampler::unbind(uint32_t texUnit) {
    glBindSampler(static_cast<GLuint>(texUnit), static_cast<GLuint>(0));
}
} // namespace gfx
