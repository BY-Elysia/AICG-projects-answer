#pragma once

#include <memory>
#include <common/application.h>
#include <scene/camera/camera.h>
#include <scene/light/light.h>
#include <graphics/gl_program.h>
#include "fullscreen_quad.h"
#include "cpu_rasterizer.h"
#include "gpu_rasterizer.h"
#include "raster_scene.h"

class RasterizerApp : public Application {
public:
    explicit RasterizerApp(Config const& config);
    ~RasterizerApp() override;

    // Render identical fixed inputs with both backends and check their HDR outputs.
    void validateBackends();

private:
    enum class RasterBackend { Cpu = 0, Gpu = 1 };

    std::unique_ptr<RasterScene> m_scene;
    std::unique_ptr<PerspectiveCamera> m_camera;
    std::unique_ptr<CpuRasterizer> m_cpuRasterizer;
    std::unique_ptr<GpuRasterizer> m_gpuRasterizer;
    std::unique_ptr<gfx::GLProgram> m_displayProgram;
    std::unique_ptr<FullscreenQuad> m_quad;
    RasterBackend m_rasterBackend = RasterBackend::Gpu;
    RasterizerStats m_stats;
    float m_elapsedSeconds = 0.0f;
    float m_titleSeconds = 0.0f;
    uint32_t m_titleFrames = 0;
    DirectionalLight m_directionalLight;

    void handleEvent(Event& event) override;
    void renderFrame() override;
    void renderUi();
};
