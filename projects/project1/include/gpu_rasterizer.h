#pragma once

#include <memory>
#include <array>
#include <string>
#include <vector>

#include <graphics/gl_program.h>
#include "shader_utils.h"
#include "rasterizer.h"

class GpuRasterizer final : public Rasterizer {
public:
    GpuRasterizer(
        const std::string& binningShader, const std::string& visibilityShader,
        const std::string& shadingShader);

    ~GpuRasterizer();

    RasterizerStats render(
        const std::vector<PreparedTriangle>& triangles, const PbrMaterial& material,
        const FrameState& frame) override;

private:
    // Triangle-driven pass that builds one triangle list for each screen tile.
    std::unique_ptr<gfx::GLProgram> m_binningProgram;
    // Tile-driven pass where one workgroup owns every pixel of one screen tile.
    std::unique_ptr<gfx::GLProgram> m_visibilityProgram;
    // Pixel-driven PBR reconstruction and imageStore pass.
    std::unique_ptr<gfx::GLProgram> m_shadingProgram;
    std::array<int, 5> m_hasTextureLocations;
    // Number of 16x16 tiles required to cover m_extent.
    glm::ivec2 m_tileExtent{0};
    // std430 PreparedTriangle[] SSBO at binding 0.
    GLuint m_triangleBuffer = 0;
    // R32F depth gfx::Texture2D retaining full floating-point normalized depth per pixel.
    std::unique_ptr<gfx::Texture2D> m_depthTexture;
    // R32UI primitive-ID gfx::Texture2D used to find interpolation attributes in the shading pass.
    std::unique_ptr<gfx::Texture2D> m_ownerTexture;
    // One atomic triangle-reference count for each screen tile.
    GLuint m_tileCountBuffer = 0;
    // Fixed-capacity triangle-index lists laid out consecutively by tile.
    GLuint m_tileIndexBuffer = 0;
    // Measures both compute passes as one GPU operation.
    GLuint m_timeQuery = 0;

    // Pixel dimensions owned by a visibility workgroup.
    static constexpr int TileSize = 16;
    // Maximum triangle references retained by one tile.
    static constexpr int MaxTrianglesPerTile = 1024;

private:
    // Recreates GPU-only visibility images and tile-list buffers after Rasterizer resizes its output image.
    void resizeBackend() override;
};
