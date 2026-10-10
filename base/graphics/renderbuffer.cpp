#include "graphics/renderbuffer.h"

#include <stdexcept>
#include "graphics/gl_utility.h"

namespace gfx {
static GLenum toNativeFormat(Renderbuffer::Format format) {
    // Internal Format       Base Format        Red   Green   Blue    Alpha
    // GL_R8                 GL_RED               8
    // GL_R8UI               GL_RED_INTEGER     ui8
    // GL_R8I                GL_RED_INTEGER      i8
    // GL_R16UI              GL_RED_INTEGER    ui16
    // GL_R16I               GL_RED_INTEGER     i16
    // GL_R32UI              GL_RED_INTEGER    ui32
    // GL_R32I               GL_RED_INTEGER     i32
    // GL_RG8                GL_RG                8      8
    // GL_RG8UI              GL_RG_INTEGER      ui8    ui8
    // GL_RG8I               GL_RG_INTEGER       i8     i8
    // GL_RG16UI             GL_RG_INTEGER     ui16   ui16
    // GL_RG16I              GL_RG_INTEGER      i16    i16
    // GL_RG32UI             GL_RG_INTEGER     ui32   ui32
    // GL_RG32I              GL_RG_INTEGER      i32    i32
    // GL_RGB8               GL_RGB               8      8      8
    // GL_RGB565             GL_RGB               5      6      5
    // GL_RGBA8              GL_RGBA              8      8      8      8
    // GL_SRGB8_ALPHA8       GL_RGBA              8      8      8      8
    // GL_RGB5_A1            GL_RGBA              5      5      5      1
    // GL_RGBA4              GL_RGBA              4      4      4      4
    // GL_RGB10_A2           GL_RGBA             10     10     10      2
    // GL_RGBA8UI            GL_RGBA_INTEGER    ui8    ui8    ui8    ui8
    // GL_RGBA8I             GL_RGBA_INTEGER     i8     i8     i8     i8
    // GL_RGB10_A2UI         GL_RGBA_INTEGER   ui10   ui10   ui10    ui2
    // GL_RGBA16UI           GL_RGBA_INTEGER   ui16   ui16   ui16   ui16
    // GL_RGBA16I            GL_RGBA_INTEGER    i16    i16    i16    i16
    // GL_RGBA32I            GL_RGBA_INTEGER    i32    i32    i32    i32
    // GL_RGBA32UI           GL_RGBA_INTEGER    i32   ui32   ui32   ui32

    // Internal Format    Base Format        Depth      Stencil
    // GL_DEPTH_COMPONENT16  GL_DEPTH_COMPONENT      16
    // GL_DEPTH_COMPONENT24  GL_DEPTH_COMPONENT      24
    // GL_DEPTH_COMPONENT32F GL_DEPTH_COMPONENT     f32
    // GL_DEPTH24_STENCIL8   GL_DEPTH_STENCIL        24          8
    // GL_DEPTH32F_STENCIL8  GL_DEPTH_STENCIL       f32          8
    // GL_STENCIL_INDEX8     GL_STENCIL               8
    switch (format) {
    case Renderbuffer::Format::R8:
        return GL_R8;
    case Renderbuffer::Format::R8UI:
        return GL_R8UI;
    case Renderbuffer::Format::R8I:
        return GL_R8I;
    case Renderbuffer::Format::R16UI:
        return GL_R16UI;
    case Renderbuffer::Format::R16I:
        return GL_R16I;
    case Renderbuffer::Format::R32UI:
        return GL_R32UI;
    case Renderbuffer::Format::R32I:
        return GL_R32I;
    case Renderbuffer::Format::RG8:
        return GL_RG8;
    case Renderbuffer::Format::RG8UI:
        return GL_RG8UI;
    case Renderbuffer::Format::RG8I:
        return GL_RG8I;
    case Renderbuffer::Format::RG16UI:
        return GL_RG16UI;
    case Renderbuffer::Format::RG16I:
        return GL_RG16I;
    case Renderbuffer::Format::RG32UI:
        return GL_RG32UI;
    case Renderbuffer::Format::RG32I:
        return GL_RG32I;
    case Renderbuffer::Format::RGB8:
        return GL_RGB8;
    case Renderbuffer::Format::RGBA8:
        return GL_RGBA8;
    case Renderbuffer::Format::SRGBA8:
        return GL_SRGB8_ALPHA8;
    case Renderbuffer::Format::RGBA8UI:
        return GL_RGBA8UI;
    case Renderbuffer::Format::RGBA8I:
        return GL_RGBA8I;
    case Renderbuffer::Format::RGBA16UI:
        return GL_RGBA16UI;
    case Renderbuffer::Format::RGBA16I:
        return GL_RGBA16I;
    case Renderbuffer::Format::RGBA32I:
        return GL_RGBA32I;
    case Renderbuffer::Format::RGBA32UI:
        return GL_RGBA32UI;
    case Renderbuffer::Format::RGB565:
        return GL_RGB565;
    case Renderbuffer::Format::RGB5A1:
        return GL_RGB5_A1;
    case Renderbuffer::Format::RGBA4:
        return GL_RGBA4;
    case Renderbuffer::Format::RGB10A2:
        return GL_RGB10_A2;
    case Renderbuffer::Format::RGB10A2UI:
        return GL_RGB10_A2UI;
    case Renderbuffer::Format::Depth16:
        return GL_DEPTH_COMPONENT16;
    case Renderbuffer::Format::Depth24:
        return GL_DEPTH_COMPONENT24;
    case Renderbuffer::Format::Depth32F:
        return GL_DEPTH_COMPONENT32F;
    case Renderbuffer::Format::Stencil8:
        return GL_STENCIL_INDEX8;
    case Renderbuffer::Format::Depth24Stencil8:
        return GL_DEPTH24_STENCIL8;
    case Renderbuffer::Format::Depth32FStencil8:
        return GL_DEPTH32F_STENCIL8;
    }

    throw std::logic_error(std::string("Unsupported format"));

    return 0;
}

Renderbuffer::Renderbuffer(uint32_t width, uint32_t height, Format format, uint32_t samples)
    : m_format{format}
    , m_width{width}
    , m_height{height}
    , m_samples{samples} {
    auto internalFormat{toNativeFormat(format)};

    glGenRenderbuffers(1, &m_handle);
    glBindRenderbuffer(GL_RENDERBUFFER, m_handle);
    if (samples > 1) {
        glRenderbufferStorageMultisample(
            GL_RENDERBUFFER, GLsizei(samples), internalFormat, GLsizei(width), GLsizei(height));
    }
    else {
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, GLsizei(width), GLsizei(height));
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

Renderbuffer::~Renderbuffer() {
    if (isValid()) {
        glDeleteRenderbuffers(1, &m_handle);
    }
}

Renderbuffer& Renderbuffer::operator=(Renderbuffer&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            glDeleteRenderbuffers(1, &m_handle);
        }

        RHIResource::operator=(std::move(rhs));
    }

    return *this;
}

void Renderbuffer::resize(uint32_t width, uint32_t height) {
    glBindRenderbuffer(GL_RENDERBUFFER, m_handle);

    const auto format = toNativeFormat(m_format);
    if (m_samples > 1) {
        glRenderbufferStorageMultisample(
            GL_RENDERBUFFER, GLsizei(m_samples), format, GLsizei(width), GLsizei(height));
    } else {
        glRenderbufferStorage(
            GL_RENDERBUFFER, format, GLsizei(width), GLsizei(height));
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    m_width = width;
    m_height = height;
}
} // namespace gfx
