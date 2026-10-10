#pragma once

#include <cstddef>
#include <cstdint>

#include "common/enum.h"
#include "graphics/rhi_resource.h"

namespace gfx {
/*
 * @brief    Encapsulate the buffer storage in the GPU global memory
 * @details  The reason why we combine all buffer type into one buffer class is that
 *           a buffer can be used as different types in different context.
 *           For example, a vertex buffer can be used as a shader storage buffer.
 *           Besides, the buffer usage should be specified at buffer creation in Vulkan.
 */
class Buffer : public RHIResource {
public:
    enum class Usage {
        StaticDraw,
        DynamicDraw,
        StreamDraw,
        StaticRead,
        DynamicRead,
        StreamRead,
        StaticCopy,
        DynamicCopy,
        StreamCopy,
    };

    enum class Access {
        ReadOnly,
        WriteOnly,
        ReadWrite,
    };

    enum class StorageFlags : uint32_t {
        None = 0u,
        Dynamic = makeBitmaskBit(0u),
        MapRead = makeBitmaskBit(1u),
        MapWrite = makeBitmaskBit(2u),
        MapPersistent = makeBitmaskBit(3u),
        MapCoherent = makeBitmaskBit(4u),
        Client = makeBitmaskBit(5u),
    };

    enum class MapFlags : uint32_t {
        Read = makeBitmaskBit(0u),
        Write = makeBitmaskBit(1u),
        Persistent = makeBitmaskBit(2u),
        Coherent = makeBitmaskBit(3u),
        InvalidateRange = makeBitmaskBit(4u),
        InvalidateBuffer = makeBitmaskBit(5u),
        FlushExplicit = makeBitmaskBit(6u),
        Unsynchronized = makeBitmaskBit(7u),
    };

    enum class UsageFlags : uint32_t {
        Undefined = makeBitmaskBit(0u),
        VertexBuffer = makeBitmaskBit(1u),
        AtomicCounterBuffer = makeBitmaskBit(2u),
        CopyRead = makeBitmaskBit(3u),
        CopyWrite = makeBitmaskBit(4u),
        IndexBuffer = makeBitmaskBit(5u),
        DispatchIndirectBuffer = makeBitmaskBit(6u),
        DrawIndirectBuffer = makeBitmaskBit(7u),
        PixelPackBuffer = makeBitmaskBit(8u),
        PixelUnPackBuffer = makeBitmaskBit(9u),
        QueryBuffer = makeBitmaskBit(10u),
        ShaderStorageBuffer = makeBitmaskBit(11u),
        TextureBuffer = makeBitmaskBit(12u),
        TransformFeedbackBuffer = makeBitmaskBit(13u),
        UniformBuffer = makeBitmaskBit(14u),
    };

public:
    Buffer(UsageFlags usageFlags, size_t size, Usage usage, void const* data = nullptr);

    Buffer(UsageFlags usageFlags, size_t size, StorageFlags storageFlags, void const* data = nullptr);

    Buffer(Buffer&& rhs) noexcept;

    ~Buffer();

    Buffer& operator=(Buffer&& rhs) noexcept;

    void resize(size_t size, Usage usage);

    void read(void* data) const;

    void read(int offset, size_t size, void* data) const;

    void write(void const* data) const;

    void write(int offset, size_t size, void const* data) const;

    void* map(Access access) const;

    void unmap() const;

    void* mapRange(intptr_t offset, size_t size, MapFlags mapFlags) const;

    void flushMappedRange(intptr_t offset, size_t size) const;

    void bindAsAtomicCounter(uint32_t binding) const;

    void bindAsTransformFeedback(uint32_t binding) const;

    void bindAsUniform(uint32_t binding) const;

    void bindAsShaderStorage(uint32_t binding) const;

    static void unbindAsAtomicCounter(uint32_t binding);

    static void unbindAsTransformFeedback(uint32_t binding);

    static void unbindAsUniform(uint32_t binding);

    static void unbindAsShaderStorage(uint32_t binding);

    void bindRangeAsAtomicCounter(uint32_t binding, intptr_t offset, size_t size) const;

    void bindRangeAsTransformFeedback(uint32_t binding, intptr_t offset, size_t size) const;

    void bindRangeAsUniform(uint32_t binding, intptr_t offset, size_t size) const;

    void bindRangeAsShaderStorage(uint32_t binding, intptr_t offset, size_t size) const;

    size_t getSize() const noexcept {
        return m_size;
    }

    bool isImmutable() const noexcept {
        return m_immutable;
    };

private:
    size_t m_size{0};
    UsageFlags m_usageFlags{0u};
    bool m_immutable{false};
};

ENABLE_BITMASK_OPERATION(Buffer::StorageFlags);
ENABLE_BITMASK_OPERATION(Buffer::MapFlags);
ENABLE_BITMASK_OPERATION(Buffer::UsageFlags);
} // namespace gfx
