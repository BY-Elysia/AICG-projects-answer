#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>
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

    void render(std::vector<DrawItem> const& draws, FrameState const& frame) override;

private:
    std::unique_ptr<gfx::GLProgram> m_transformProgram;
    std::unique_ptr<gfx::GLProgram> m_rasterProgram;
    std::unique_ptr<gfx::Buffer> m_transformedBuffer;
    gfx::TransformFeedback m_transformFeedback;
    gfx::VertexArray m_feedbackVao;
    std::unique_ptr<gfx::Framebuffer> m_framebuffer;
    std::unique_ptr<gfx::Renderbuffer> m_depthBuffer;
    void resizeBackend() override;
    void transformVertices(Mesh const& mesh, glm::mat4 const& transform, FrameState const& frame);
    void setMaterial(PbrMaterial const& material);
};
