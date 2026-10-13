#pragma once

#include <glad/glad.h>
#include <graphics/buffer.h>
#include <graphics/vertex_array.h>

class FullscreenQuad {
public:
    FullscreenQuad()
        : m_buffer{gfx::Buffer::UsageFlags::VertexBuffer, sizeof(Vertices),
                   gfx::Buffer::Usage::StaticDraw, Vertices} {
        m_array.setFloatAttribute(m_buffer, {0, 2, gfx::VertexArray::DataType::Float32,
                                            4 * sizeof(float), 0});
        m_array.setFloatAttribute(m_buffer, {1, 2, gfx::VertexArray::DataType::Float32,
                                            4 * sizeof(float), 2 * sizeof(float)});
    }

    void draw() const {
        m_array.bind();
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gfx::VertexArray::unbind();
    }

private:
    static constexpr float Vertices[] = {
        -1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1, 1, 1, 1, 1};
    gfx::Buffer m_buffer;
    gfx::VertexArray m_array;
};
