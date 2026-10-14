#pragma once

#include <cstdint>

#include "graphics/buffer.h"
#include "graphics/rhi.h"
#include "graphics/rhi_resource.h"

namespace gfx {
class TransformFeedback : public RHIResource {
public:
    TransformFeedback();

    TransformFeedback(TransformFeedback&& rhs) noexcept = default;

    ~TransformFeedback();

    TransformFeedback& operator=(TransformFeedback&& rhs) noexcept;

    void setBuffer(uint32_t binding, Buffer const& buffer) const;

    void bind() const;

    static void unbind();

    // Capture category: Points, Lines, or Triangles.
    void begin(RHI::Primitive primitive) const;

    static void end();

    // Draw captured vertices with any RHI primitive mode supported by glDrawTransformFeedback.
    void draw(RHI::Primitive primitive) const;
};
} // namespace gfx
