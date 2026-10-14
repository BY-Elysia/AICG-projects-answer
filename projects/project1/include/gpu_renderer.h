#pragma once

#include <array>
#include <memory>
#include <string>
#include <graphics/buffer.h>
#include <graphics/framebuffer.h>
#include <graphics/gl_program.h>
#include <graphics/renderbuffer.h>
#include <graphics/timer.h>
#include <graphics/transform_feedback.h>
#include <graphics/vertex_array.h>
#include "renderer.h"

// OpenGL 4.1: indexed vertex transform, on-GPU feedback, then shader rasterization.
class GpuRenderer final : public Renderer {
public:
    GpuRenderer(std::string const& transformShader,
                std::string const& rasterVertexShader,
                std::string const& rasterGeometryShader,
                std::string const& rasterFragmentShader);

    ~GpuRenderer() override = default;

    RendererStats render(std::span<DrawItem const> draws, FrameState const& frame) override;

private:
    std::unique_ptr<gfx::GLProgram> m_transformProgram;
    std::unique_ptr<gfx::GLProgram> m_rasterProgram;
    std::unique_ptr<gfx::Buffer> m_transformedBuffer;
    gfx::TransformFeedback m_transformFeedback;
    gfx::VertexArray m_feedbackVao;
    std::array<int, 5> m_hasTextureLocations{};
    std::unique_ptr<gfx::Framebuffer> m_framebuffer;
    std::unique_ptr<gfx::Renderbuffer> m_depthBuffer;
    std::array<gfx::Timer, 3> m_timeQueries;
    std::array<bool, 3> m_queryPending{};
    float m_lastGpuMilliseconds = 0.0f;

    void resizeBackend() override;
    void transformVertices(Mesh const& mesh, glm::mat4 const& transform, FrameState const& frame);
    void setMaterial(PbrMaterial const& material);
};
