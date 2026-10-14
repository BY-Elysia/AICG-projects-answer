#include "graphics/buffer.h"

#include <stdexcept>
#include "graphics/gl_utility.h"

namespace gfx {
static GLenum toNativeUsage(Buffer::Usage usage) {
    switch (usage) {
    case Buffer::Usage::StaticDraw:
        return GL_STATIC_DRAW;
    case Buffer::Usage::DynamicDraw:
        return GL_DYNAMIC_DRAW;
    case Buffer::Usage::StreamDraw:
        return GL_STREAM_DRAW;
    case Buffer::Usage::StaticRead:
        return GL_STATIC_READ;
    case Buffer::Usage::DynamicRead:
        return GL_DYNAMIC_READ;
    case Buffer::Usage::StreamRead:
        return GL_STREAM_READ;
    case Buffer::Usage::StaticCopy:
        return GL_STATIC_COPY;
    case Buffer::Usage::DynamicCopy:
        return GL_DYNAMIC_COPY;
    case Buffer::Usage::StreamCopy:
        return GL_STATIC_COPY;
    default:
        throw std::logic_error("Unsupported usage");
    }

    return 0;
}

static GLenum toNativeAccess(Buffer::Access access) {
    switch (access) {
    case Buffer::Access::ReadOnly:
        return GL_READ_ONLY;
    case Buffer::Access::WriteOnly:
        return GL_WRITE_ONLY;
    case Buffer::Access::ReadWrite:
        return GL_READ_WRITE;
    default:
        throw std::logic_error("Unsupported access");
    }

    return 0;
}

static GLbitfield toNativeStorageFlags(Buffer::StorageFlags flags) noexcept {
    GLbitfield bitfields{0u};
    if (testBitmaskContain(flags, Buffer::StorageFlags::Dynamic)) {
        bitfields |= GL_DYNAMIC_STORAGE_BIT;
    }

    if (testBitmaskContain(flags, Buffer::StorageFlags::MapRead)) {
        bitfields |= GL_MAP_READ_BIT;
    }

    if (testBitmaskContain(flags, Buffer::StorageFlags::MapWrite)) {
        bitfields |= GL_MAP_WRITE_BIT;
    }

    if (testBitmaskContain(flags, Buffer::StorageFlags::MapPersistent)) {
        bitfields |= GL_MAP_PERSISTENT_BIT;
    }

    if (testBitmaskContain(flags, Buffer::StorageFlags::MapCoherent)) {
        bitfields |= GL_MAP_COHERENT_BIT;
    }

    if (testBitmaskContain(flags, Buffer::StorageFlags::Client)) {
        bitfields |= GL_CLIENT_STORAGE_BIT;
    }

    return bitfields;
}

static GLbitfield toNativeMapFlags(Buffer::MapFlags flags) noexcept {
    GLbitfield bitfields{0u};
    if (testBitmaskContain(flags, Buffer::MapFlags::Read)) {
        bitfields |= GL_MAP_READ_BIT;
    }

    if (testBitmaskContain(flags, Buffer::MapFlags::Write)) {
        bitfields |= GL_MAP_WRITE_BIT;
    }

    if (testBitmaskContain(flags, Buffer::MapFlags::Persistent)) {
        bitfields |= GL_MAP_PERSISTENT_BIT;
    }

    if (testBitmaskContain(flags, Buffer::MapFlags::Coherent)) {
        bitfields |= GL_MAP_COHERENT_BIT;
    }

    if (testBitmaskContain(flags, Buffer::MapFlags::InvalidateRange)) {
        bitfields |= GL_MAP_INVALIDATE_RANGE_BIT;
    }

    if (testBitmaskContain(flags, Buffer::MapFlags::InvalidateBuffer)) {
        bitfields |= GL_MAP_INVALIDATE_BUFFER_BIT;
    }

    if (testBitmaskContain(flags, Buffer::MapFlags::FlushExplicit)) {
        bitfields |= GL_MAP_FLUSH_EXPLICIT_BIT;
    }

    if (testBitmaskContain(flags, Buffer::MapFlags::Unsynchronized)) {
        bitfields |= GL_MAP_UNSYNCHRONIZED_BIT;
    }

    return bitfields;
}

Buffer::Buffer(UsageFlags usageFlags, size_t size, Usage usage, void const* data)
    : m_usageFlags{usageFlags}
    , m_size{size}
    , m_immutable{false} {
    glGenBuffers(1, &m_handle);
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    glBufferData(GL_COPY_WRITE_BUFFER, size, data, toNativeUsage(usage));
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

Buffer::Buffer(UsageFlags usageFlags, size_t size, StorageFlags storageFlags, void const* data)
    : m_usageFlags{usageFlags}
    , m_size{size}
    , m_immutable{true} {
    glGenBuffers(1, &m_handle);
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    glBufferStorage(GL_COPY_WRITE_BUFFER, size, data, toNativeStorageFlags(storageFlags));
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

Buffer::Buffer(Buffer&& rhs) noexcept
    : RHIResource{std::move(rhs)}
    , m_usageFlags{rhs.m_usageFlags}
    , m_size{rhs.m_size}
    , m_immutable{rhs.m_immutable} {
    rhs.m_size = 0;
}

Buffer::~Buffer() {
    if (m_handle) {
        glDeleteBuffers(1, &m_handle);
    }
}

Buffer& Buffer::operator=(Buffer&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            glDeleteBuffers(1, &m_handle);
        }

        m_size = rhs.m_size;
        m_usageFlags = rhs.m_usageFlags;
        m_immutable = rhs.m_immutable;

        RHIResource::operator=(std::move(rhs));
    }

    return *this;
}

void Buffer::resize(size_t size, Usage usage) {
    if (m_immutable) {
        throw std::logic_error("Cannot resize immutable buffer");
    }

    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    glBufferData(GL_COPY_WRITE_BUFFER, size, nullptr, toNativeUsage(usage));
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    m_size = size;
}

void Buffer::read(void* data) const {
    glBindBuffer(GL_COPY_READ_BUFFER, m_handle);
    glGetBufferSubData(GL_COPY_READ_BUFFER, 0, GLsizeiptr(m_size), data);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
}

void Buffer::read(int offset, size_t size, void* data) const {
    glBindBuffer(GL_COPY_READ_BUFFER, m_handle);
    glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(offset), GLsizeiptr(size), data);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
}

void Buffer::write(void const* data) const {
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    glBufferSubData(GL_COPY_WRITE_BUFFER, 0, GLsizeiptr(m_size), data);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Buffer::write(int offset, size_t size, void const* data) const {
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    glBufferSubData(GL_COPY_WRITE_BUFFER, GLintptr(offset), GLsizeiptr(size), data);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void* Buffer::map(Access access) const {
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    void* data{glMapBuffer(GL_COPY_WRITE_BUFFER, toNativeAccess(access))};
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    return data;
}

void Buffer::unmap() const {
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    glUnmapBuffer(GL_COPY_WRITE_BUFFER);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void* Buffer::mapRange(intptr_t offset, size_t size, MapFlags mapFlags) const {
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    void* data{glMapBufferRange(GL_COPY_WRITE_BUFFER, offset, size, toNativeMapFlags(mapFlags))};
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    return data;
}

void Buffer::flushMappedRange(intptr_t offset, size_t size) const {
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_handle);
    glFlushMappedBufferRange(GL_COPY_WRITE_BUFFER, offset, size);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Buffer::bindAsAtomicCounter(uint32_t binding) const {
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, binding, m_handle);
}

void Buffer::bindAsTransformFeedback(uint32_t binding) const {
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, binding, m_handle);
}

void Buffer::bindAsUniform(uint32_t binding) const {
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_handle);
}

void Buffer::bindAsShaderStorage(uint32_t binding) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_handle);
}

void Buffer::unbindAsAtomicCounter(uint32_t binding) {
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, binding, 0);
}

void Buffer::unbindAsTransformFeedback(uint32_t binding) {
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, binding, 0);
}

void Buffer::unbindAsUniform(uint32_t binding) {
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, 0);
}

void Buffer::unbindAsShaderStorage(uint32_t binding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, 0);
}

void Buffer::bindRangeAsAtomicCounter(uint32_t binding, intptr_t offset, size_t size) const {
    glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, binding, m_handle, GLintptr(offset), GLsizeiptr(size));
}

void Buffer::bindRangeAsTransformFeedback(uint32_t binding, intptr_t offset, size_t size) const {
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, binding, m_handle, GLintptr(offset), GLsizeiptr(size));
}

void Buffer::bindRangeAsUniform(uint32_t binding, intptr_t offset, size_t size) const {
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, m_handle, GLintptr(offset), GLsizeiptr(size));
}

void Buffer::bindRangeAsShaderStorage(uint32_t binding, intptr_t offset, size_t size) const {
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, m_handle, GLintptr(offset), GLsizeiptr(size));
}
} // namespace gfx
