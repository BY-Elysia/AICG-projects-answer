#pragma once

#include <cstddef>
#include <cstdint>

#include "graphics/buffer.h"
#include "graphics/rhi_resource.h"

namespace gfx {
class VertexArray : public RHIResource {
public:
    enum class DataType { Int8, UInt8, Int16, UInt16, Int32, UInt32, Float16, Float32, Float64 };

    struct VertexAttribute {
        uint32_t location;
        size_t componentCount;
        DataType dataType;
        size_t stride;
        intptr_t offset{0};
        uint32_t divisor{0};
    };

public:
    VertexArray();

    VertexArray(VertexArray&& rhs) noexcept;

    ~VertexArray();

    VertexArray& operator=(VertexArray&& rhs) noexcept;

    void bind() const;

    static void unbind();

    void setFloatAttribute(Buffer const& buffer, VertexAttribute const& attribute, bool normalized = false);

    void setIntegerAttribute(Buffer const& buffer, VertexAttribute const& attribute);

    void setDoubleAttribute(Buffer const& buffer, VertexAttribute const& attribute);

    void setIndexBuffer(Buffer const& buffer);
};
} // namespace gfx
