#include "graphics/transform_feedback.h"

#include <stdexcept>
#include <utility>
#include "graphics/gl_utility.h"

namespace gfx {

static GLenum toNativePrimitive(RHI::Primitive primitive) {
    switch (primitive) {
    case RHI::Primitive::Points:
        return GL_POINTS;
    case RHI::Primitive::Lines:
        return GL_LINES;
    case RHI::Primitive::LineStrip:
        return GL_LINE_STRIP;
    case RHI::Primitive::LineLoop:
        return GL_LINE_LOOP;
    case RHI::Primitive::Triangles:
        return GL_TRIANGLES;
    case RHI::Primitive::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    case RHI::Primitive::TriangleFan:
        return GL_TRIANGLE_FAN;
    }
    throw std::invalid_argument("Unsupported transform feedback draw primitive");
}

TransformFeedback::TransformFeedback() {
    glGenTransformFeedbacks(1, &m_handle);
}

TransformFeedback::~TransformFeedback() {
    if (isValid()) {
        glDeleteTransformFeedbacks(1, &m_handle);
    }
}

TransformFeedback& TransformFeedback::operator=(TransformFeedback&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            glDeleteTransformFeedbacks(1, &m_handle);
        }

        RHIResource::operator=(std::move(rhs));
    }

    return *this;
}

void TransformFeedback::setBuffer(uint32_t binding, Buffer const& buffer) const {
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, m_handle);
    buffer.bindAsTransformFeedback(binding);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
}

void TransformFeedback::bind() const {
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, m_handle);
}

void TransformFeedback::unbind() {
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
}

void TransformFeedback::begin(RHI::Primitive primitive) const {
    if (primitive != RHI::Primitive::Points && primitive != RHI::Primitive::Lines &&
        primitive != RHI::Primitive::Triangles) {
        throw std::invalid_argument("Transform feedback capture requires points, lines, or triangles");
    }
    GLenum const mode = toNativePrimitive(primitive);
    bind();
    glBeginTransformFeedback(mode);
}

void TransformFeedback::end() {
    glEndTransformFeedback();
}

void TransformFeedback::draw(RHI::Primitive primitive) const {
    glDrawTransformFeedback(toNativePrimitive(primitive), m_handle);
}

} // namespace gfx
