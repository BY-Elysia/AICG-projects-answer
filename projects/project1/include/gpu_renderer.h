#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <glad/glad.h>
#include <graphics/buffer.h>
#include <graphics/framebuffer.h>
#include <graphics/gl_program.h>
#include <graphics/vertex_array.h>
#include "renderer.h"

// OpenGL 4.1: vertex shader transforms inputs; CPU clips/bins; fragment shaders rasterize/shade.
class GpuRenderer final : public Renderer {
public:
    GpuRenderer(std::string const& transformShader,
                std::string const& fullscreenShader,
                std::string const& visibilityShader,
                std::string const& shadingShader);

    ~GpuRenderer() override;

    RendererStats render(std::span<DrawItem const> draws, FrameState const& frame) override;

private:
    std::unique_ptr<gfx::GLProgram> m_transformProgram;
    std::unique_ptr<gfx::Buffer> m_vertexBuffer;
    std::unique_ptr<gfx::Buffer> m_transformedBuffer;
    gfx::VertexArray m_vertexVao;
    GLuint m_transformFeedback = 0;
    GLuint m_vertexTimeQuery = 0;
    std::unique_ptr<gfx::GLProgram> m_visibilityProgram;
    std::unique_ptr<gfx::GLProgram> m_shadingProgram;
    std::array<int, 5> m_hasTextureLocations;
    gfx::VertexArray m_fullscreenVao;
    // Packed RGBA32F records: one PreparedTriangle occupies fourteen texels.
    std::unique_ptr<gfx::Texture2D> m_triangleTexture;
    // RG32UI (offset, count) for every tile and packed R32UI primitive indices.
    std::unique_ptr<gfx::Texture2D> m_tileRangeTexture;
    std::unique_ptr<gfx::Texture2D> m_tileIndexTexture;
    std::unique_ptr<gfx::Texture2D> m_ownerTexture;
    std::unique_ptr<gfx::Framebuffer> m_visibilityFramebuffer;
    std::unique_ptr<gfx::Framebuffer> m_shadingFramebuffer;
    GLuint m_timeQuery = 0;
    int m_maxTextureSize = 0;
    static constexpr int TileSize = 16;

    void resizeBackend() override;
    std::vector<TransformedVertex>
    transformVertices(Mesh const& mesh, glm::mat4 const& transform, FrameState const& frame, float& milliseconds);
    void uploadPacked(std::unique_ptr<gfx::Texture2D>& texture,
                      size_t texelCount,
                      gfx::Texture::Format format,
                      gfx::Texture::ExternalFormat pixelFormat,
                      gfx::Texture::PixelType type,
                      size_t texelBytes,
                      void const* data);
};
