#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <graphics/buffer.h>
#include <graphics/framebuffer.h>
#include <graphics/gl_program.h>
#include <graphics/renderbuffer.h>
#include <graphics/transform_feedback.h>
#include <graphics/vertex_array.h>
#include "renderer.h"

class GpuRenderer final : public Renderer {
public:
    GpuRenderer(uint32_t width,
                uint32_t height,
                std::filesystem::path const& transformShader,
                std::filesystem::path const& rasterVertexShader,
                std::filesystem::path const& rasterGeometryShader,
                std::filesystem::path const& rasterFragmentShader);

    ~GpuRenderer() override = default;

    void clear() override;

    void render(DrawItem const& drawItem, FrameState const& frame) override;

private:
    std::unique_ptr<gfx::GLProgram> m_transformProgram;
    gfx::VertexArray m_feedbackVao;
    gfx::TransformFeedback m_transformFeedback;
    std::unique_ptr<gfx::Buffer> m_transformedBuffer;

    std::unique_ptr<gfx::GLProgram> m_rasterProgram;

    std::unique_ptr<gfx::Framebuffer> m_framebuffer;
    std::unique_ptr<gfx::Renderbuffer> m_depthBuffer;

private:
    void resizeBackend() override;

    void transformVertices(Mesh const& mesh, FrameState const& frame);

    void setMaterial(PbrMaterial const& material);
};
