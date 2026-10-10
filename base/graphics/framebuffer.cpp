#include "graphics/framebuffer.h"
#include "graphics/gl_utility.h"
#include "graphics/texture/texture_utility.h"

namespace gfx {
Framebuffer::Framebuffer() {
    glGenFramebuffers(1, &m_handle);
}

Framebuffer::~Framebuffer() {
    if (m_handle != 0) {
        glDeleteFramebuffers(1, &m_handle);
    }
}

Framebuffer& Framebuffer::operator=(Framebuffer&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            glDeleteFramebuffers(1, &m_handle);
        }

        RHIResource::operator=(std::move(rhs));
    }

    return *this;
}

void Framebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
}

void Framebuffer::bindRead() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_handle);
}

void Framebuffer::bindWrite() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(0));
}

void Framebuffer::unbindRead() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(0));
}

void Framebuffer::unbindWrite() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(0));
}

void Framebuffer::attachAsColor(uint32_t index, Texture const& texture, int level) {
    GLenum const attachment{GL_COLOR_ATTACHMENT0 + index};
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture.getNativeHandle(), level);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachAsDepth(Texture const& texture, int level) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture.getNativeHandle(), level);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachAsStencil(Texture const& texture, int level) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, texture.getNativeHandle(), level);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachAsDepthStencil(Texture const& texture, int level) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture.getNativeHandle(), level);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachAsColor(uint32_t index, Renderbuffer const& rbo) {
    GLenum const attachment{GL_COLOR_ATTACHMENT0 + index};
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, rbo.getNativeHandle());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachAsDepth(Renderbuffer const& rbo) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo.getNativeHandle());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachAsStencil(Renderbuffer const& rbo) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo.getNativeHandle());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachAsDepthStencil(Renderbuffer const& rbo) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo.getNativeHandle());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachLayerAsColor(uint32_t index, Texture const& texture, int layer, int level) {
    GLenum const attachment{GL_COLOR_ATTACHMENT0 + index};
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture.getNativeHandle(), level, layer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachLayerAsDepth(Texture const& texture, int layer, int level) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture.getNativeHandle(), level, layer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachLayerAsStencil(Texture const& texture, int layer, int level) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, texture.getNativeHandle(), level, layer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::attachLayerAsDepthStencil(Texture const& texture, int layer, int level) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture.getNativeHandle(), level, layer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Framebuffer::isComplete() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    bool const isComplete{glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE};
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return isComplete;
}

std::string Framebuffer::getDiagnostic() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
    GLenum status{glCheckFramebufferStatus(GL_FRAMEBUFFER)};
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
        return "framebuffer: complete";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "framebuffer: incomplete attachment";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "framebuffer: missing attachment";
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        return "framebuffer: incomplete draw buffer";
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        return "framebuffer: incomplete layer targets";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        return "framebuffer: incomplete multisample";
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        return "framebuffer: incomplete read buffer";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "framebuffer: unsupported";
    case GL_FRAMEBUFFER_UNDEFINED:
        return "framebuffer: undefined";
    }

    return "framebuffer: unknown";
}

void Framebuffer::setColorWrite(uint32_t index) const {
    GLenum colorAttachment{GL_COLOR_ATTACHMENT0 + index};
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glDrawBuffers(1, &colorAttachment);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::setColorWrites(std::vector<uint32_t> const& indices) {
    std::vector<GLenum> colorAttachments;
    colorAttachments.reserve(indices.size());
    for (auto index : indices) {
        colorAttachments.push_back(GL_COLOR_ATTACHMENT0 + index);
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glDrawBuffers(GLsizei(colorAttachments.size()), colorAttachments.data());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::disableColorWrite() {
    GLenum buffer{GL_NONE};
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glDrawBuffers(1, &buffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::setColorRead(uint32_t index) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_handle);
    glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void Framebuffer::disableColorRead() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_handle);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void Framebuffer::clearColorBuffer(uint32_t index, glm::ivec4 const& color) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glClearBufferiv(GL_COLOR, index, &color[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::clearColorBuffer(uint32_t index, glm::uvec4 const& color) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glClearBufferuiv(GL_COLOR, index, &color[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::clearColorBuffer(uint32_t index, glm::vec4 const& color) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glClearBufferfv(GL_COLOR, index, &color[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::clearDepthBuffer(float depth) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glClearBufferfv(GL_DEPTH, 0, &depth);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::clearStencilBuffer(int stencil) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glClearBufferiv(GL_STENCIL, 0, &stencil);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::clearDepthStencilBuffer(float depth, int stencil) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_handle);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::readBackBuffer() {
    glReadBuffer(GL_BACK);
}

void Framebuffer::readPixels(
    int x, int y, int width, int height, Texture::ExternalFormat format, Texture::PixelType type, void* data) {
    // Note we need to set read buffer before read pixel: glReadBuffer(xxx);
    // Also, don't forget to bind the framebuffer
    GLenum glFormat{details::toNativeExternalFormat(format)};
    GLenum glType{details::toNativeDataType(type)};
    glReadPixels(x, y, width, height, glFormat, glType, data);
}
} // namespace gfx
