#include "graphics/gl_utility.h"
#include "graphics/vertex_array.h"

#include <stdexcept>

namespace gfx {
static GLenum toNativeDataType(VertexArray::DataType type) {
    switch (type) {
    case VertexArray::DataType::Int8:
        return GL_BYTE;
    case VertexArray::DataType::UInt8:
        return GL_UNSIGNED_BYTE;
    case VertexArray::DataType::Int16:
        return GL_SHORT;
    case VertexArray::DataType::UInt16:
        return GL_UNSIGNED_SHORT;
    case VertexArray::DataType::Int32:
        return GL_INT;
    case VertexArray::DataType::UInt32:
        return GL_UNSIGNED_INT;
    case VertexArray::DataType::Float16:
        return GL_HALF_FLOAT;
    case VertexArray::DataType::Float32:
        return GL_FLOAT;
    case VertexArray::DataType::Float64:
        return GL_DOUBLE;
    }

    throw std::runtime_error("Unsupported data type");

    return 0;
}

VertexArray::VertexArray() {
    glGenVertexArrays(1, &m_handle);
}

VertexArray::VertexArray(VertexArray&& rhs) noexcept
    : RHIResource{std::move(rhs)} {}

VertexArray::~VertexArray() {
    if (isValid()) {
        glDeleteVertexArrays(1, &m_handle);
    }
}

VertexArray& VertexArray::operator=(VertexArray&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            glDeleteVertexArrays(1, &m_handle);
        }

        RHIResource::operator=(std::move(rhs));
    }

    return *this;
}

void VertexArray::bind() const {
    glBindVertexArray(m_handle);
}

void VertexArray::unbind() {
    glBindVertexArray(0u);
}

void VertexArray::setFloatAttribute(Buffer const& buffer, VertexAttribute const& attribute, bool normalized) {
    glBindVertexArray(m_handle);
    glBindBuffer(GL_ARRAY_BUFFER, buffer.getNativeHandle());

    void const* const offset{reinterpret_cast<void const*>(attribute.offset)};
    GLenum const type{toNativeDataType(attribute.dataType)};
    glVertexAttribPointer(attribute.location,
                          GLint(attribute.componentCount),
                          type,
                          GLboolean(normalized),
                          GLsizei(attribute.stride),
                          offset);

    glEnableVertexAttribArray(attribute.location);
    glVertexAttribDivisor(attribute.location, attribute.divisor);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void VertexArray::setIntegerAttribute(Buffer const& buffer, VertexAttribute const& attribute) {
    glBindVertexArray(m_handle);
    glBindBuffer(GL_ARRAY_BUFFER, buffer.getNativeHandle());

    void const* const offset{reinterpret_cast<void const*>(attribute.offset)};
    GLenum const type{toNativeDataType(attribute.dataType)};
    glVertexAttribIPointer(
        attribute.location, GLint(attribute.componentCount), type, GLsizei(attribute.stride), offset);

    glEnableVertexAttribArray(attribute.location);
    glVertexAttribDivisor(attribute.location, attribute.divisor);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void VertexArray::setDoubleAttribute(Buffer const& buffer, VertexAttribute const& attribute) {
    glBindVertexArray(m_handle);
    glBindBuffer(GL_ARRAY_BUFFER, buffer.getNativeHandle());

    void const* const offset{reinterpret_cast<void const*>(attribute.offset)};
    GLenum const type{toNativeDataType(attribute.dataType)};
    glVertexAttribLPointer(
        attribute.location, GLint(attribute.componentCount), type, GLsizei(attribute.stride), offset);

    glEnableVertexAttribArray(attribute.location);
    glVertexAttribDivisor(attribute.location, attribute.divisor);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void VertexArray::setIndexBuffer(Buffer const& buffer) {
    glBindVertexArray(m_handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.getNativeHandle());
    glBindVertexArray(0);
}
} // namespace gfx
