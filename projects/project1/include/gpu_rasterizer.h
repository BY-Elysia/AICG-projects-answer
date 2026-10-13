#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <graphics/framebuffer.h>
#include <graphics/gl_program.h>
#include <graphics/vertex_array.h>
#include "rasterizer.h"

// OpenGL 4.1: CPU builds tile lists; fragment shaders perform coverage, depth and PBR.
class GpuRasterizer final : public Rasterizer {
public:
    GpuRasterizer(const std::string& vertexShader, const std::string& visibilityShader,
                  const std::string& shadingShader);

    ~GpuRasterizer() override;

    RasterizerStats render(const std::vector<PreparedTriangle>& triangles,
                           const PbrMaterial& material, const FrameState& frame) override;

private:
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
    void uploadPacked(std::unique_ptr<gfx::Texture2D>& texture, size_t texelCount,
                      gfx::Texture::Format format, gfx::Texture::ExternalFormat pixelFormat,
                      gfx::Texture::PixelType type, size_t texelBytes, void const* data);
};
