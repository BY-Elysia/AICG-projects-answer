#pragma once

#include <memory>
#include <common/application.h>
#include <scene/camera/camera.h>
#include <scene/light/light.h>
#include <graphics/gl_program.h>
#include <graphics/vertex_array.h>
#include "cpu_renderer.h"
#include "gpu_renderer.h"
#include "raster_scene.h"

class SoftwareRendererApp : public Application {
public:
    explicit SoftwareRendererApp(Config const& config);
    ~SoftwareRendererApp() override;

    // Render identical fixed inputs with both backends and check their HDR outputs.
    void validateBackends();

private:
    enum class RenderBackend { Cpu = 0, Gpu = 1 };

    std::unique_ptr<RasterScene> m_scene;
    std::unique_ptr<PerspectiveCamera> m_camera;
    std::unique_ptr<CpuRenderer> m_cpuRenderer;
    std::unique_ptr<GpuRenderer> m_gpuRenderer;
    std::unique_ptr<gfx::GLProgram> m_displayProgram;
    gfx::VertexArray m_fullscreenVao;
    RenderBackend m_renderBackend = RenderBackend::Gpu;
    RendererStats m_stats;
    float m_elapsedSeconds = 0.0f;
    float m_titleSeconds = 0.0f;
    uint32_t m_titleFrames = 0;
    DirectionalLight m_directionalLight;

    void handleEvent(Event& event) override;
    void renderFrame() override;
    void renderUi();
};
