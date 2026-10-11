#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <common/application.h>
#include <graphics/framebuffer.h>
#include <graphics/gl_program.h>
#include <graphics/sampler.h>
#include <graphics/texture/texture2d.h>
#include <graphics/texture/texture_cubemap.h>
#include <graphics/vertex_array.h>
#include <scene/camera/camera.h>

#include "material.h"
#include "model.h"
#include "sphere.h"

class RayTracer final : public Application {
  public:
    explicit RayTracer(Config const& config);

    ~RayTracer() override;

  private:
    std::vector<Sphere> m_spheres;
    std::vector<Material> m_sphereMaterials;
    std::unique_ptr<Model> m_lucy;

    int m_renderSceneIndex{0};
    int m_activeSceneIndex{-1};
    bool m_uiInitialized{false};

    std::unique_ptr<PerspectiveCamera> m_camera;

    std::unique_ptr<gfx::TextureCubemap> m_skybox;

    std::unique_ptr<gfx::GLProgram> m_rayTracingProgram;

    std::unique_ptr<gfx::VertexArray> m_screenQuad;
    std::unique_ptr<gfx::GLProgram> m_presentProgram;
    std::unique_ptr<gfx::Sampler> m_nearestSampler;
    std::unique_ptr<gfx::Sampler> m_skyboxSampler;

    std::unique_ptr<gfx::Framebuffer> m_sampleFramebuffers[2];
    uint32_t m_currentReadBuffer{0};
    uint32_t m_currentWriteBuffer{1};
    uint32_t m_sampleCount{0};

    std::unique_ptr<gfx::Texture2D> m_outFrames[2];
    std::unique_ptr<gfx::Texture2D> m_rngStates[2];

    std::unique_ptr<gfx::Texture2D> m_vertexBuffer;
    std::unique_ptr<gfx::Texture2D> m_indexBuffer;

    std::unique_ptr<gfx::Texture2D> m_sphereBuffer;
    std::unique_ptr<gfx::Texture2D> m_primitiveBuffer;

    std::unique_ptr<gfx::Texture2D> m_materialBuffer;

    std::unique_ptr<gfx::Texture2D> m_bvhBuffer;

    static constexpr uint32_t kBufferWidth{2048};

  private:
    void handleEvent(Event& event) override;

    void renderFrame() override;

    void updateCamera();

    void createSpheres();

    void createRenderScene(int index);

    void createScene1();

    void createScene2();

    void createScene3();

    void createPrimitiveBuffer(std::vector<Sphere> const& spheres,
                               std::vector<Model const*> const& models,
                               std::vector<glm::mat4> const& transforms,
                               std::vector<Material> const& sphereMaterials,
                               std::vector<Material> const& modelMaterials);

    void createTextures();

    void createSkybox();

    void bindSceneTextures();

    void drawScreenQuad() const;

    void initUI();

    void renderUI();

    void resetAccumulation() noexcept { m_sampleCount = 0; }
};
