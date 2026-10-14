#include "software_renderer_app.h"

#include <stdexcept>
#include <format>
#include <glad/glad.h>
#include <common/event/event_dispatcher.h>
#include <common/event/keyboard_event.h>
#include <glm/ext.hpp>
#include <graphics/rhi.h>
#include "shader_utils.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

constexpr char const* ModelPath = "models/DamagedHelmet.gltf";
constexpr char const* TransformPath = "shaders/transform.vert";
constexpr char const* RasterVertexPath = "shaders/raster_input.vert";
constexpr char const* RasterGeometryPath = "shaders/raster_bounds.geom";
constexpr char const* ShadingPath = "shaders/raster_shading.frag";
constexpr char const* DisplayVertexPath = "shaders/display.vert";
constexpr char const* DisplayFragmentPath = "shaders/display.frag";
constexpr float ModelRotationRadiansPerSecond = 0.5f;

SoftwareRendererApp::SoftwareRendererApp(Config const& config)
    : Application(config) {
    m_scene = std::make_unique<RasterScene>(getAssetPath(ModelPath));

    m_camera = std::make_unique<PerspectiveCamera>(glm::radians(45.0f), m_window.aspectRatio(), 0.1f, 100.0f);
    m_camera->transform.position = {0.0f, 0.0f, 5.0f};

    // Configure a light above the camera-facing left/front quadrant. getFront() below supplies
    // the direction in which its rays travel, matching FrameState's direction convention.
    m_directionalLight.transform.rotation =
        glm::quatLookAt(glm::normalize(glm::vec3(0.5f, -0.5f, -0.70710678f)), Transform::getDefaultUp());
    m_directionalLight.intensity = 10.0f;

    m_cpuRenderer = std::make_unique<CpuRenderer>();
    m_gpuRenderer = std::make_unique<GpuRenderer>(getAssetPath(TransformPath),
                                                  getAssetPath(RasterVertexPath),
                                                  getAssetPath(RasterGeometryPath),
                                                  getAssetPath(ShadingPath));

    m_displayProgram = makeDisplayProgram(getAssetPath(DisplayVertexPath), getAssetPath(DisplayFragmentPath));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(m_window.handle()), true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui GLFW initialization failed");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 410")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui OpenGL initialization failed");
    }
    ImGui::GetIO().IniFilename = nullptr;
    resetClock();
}

SoftwareRendererApp::~SoftwareRendererApp() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void SoftwareRendererApp::handleEvent(Event& event) {
    EventDispatcher dispatcher{event};
    dispatcher.dispatch<KeyPressEvent>([this](KeyPressEvent& key) {
        if (key.getKeyCode() == KeyCode::Escape) {
            close();
            return true;
        }
        return false;
    });
}

void SoftwareRendererApp::renderFrame() {
    auto const width = m_window.framebufferWidth();
    auto const height = m_window.framebufferHeight();
    if (width == 0 || height == 0) {
        return;
    }
    m_camera->aspect = m_window.aspectRatio();
    m_elapsedSeconds += getDeltaTime();
    m_titleSeconds += getDeltaTime();
    ++m_titleFrames;
    if (m_titleSeconds >= 1.0f) {
        m_window.setTitle(std::format("Software Renderer | {:.1f} FPS", m_titleFrames / m_titleSeconds));
        m_titleSeconds = 0.0f;
        m_titleFrames = 0;
    }
    FrameState frame;
    frame.model =
        glm::rotate(glm::mat4(1.0f), m_elapsedSeconds * ModelRotationRadiansPerSecond, glm::vec3(0.0f, 1.0f, 0.0f));
    frame.view = m_camera->getViewMatrix();
    frame.projection = m_camera->getProjectionMatrix();
    frame.cameraPosition = m_camera->transform.position;
    frame.directionalLight = m_directionalLight;
    frame.extent = {width, height};

    Renderer& renderer = m_renderBackend == RenderBackend::Cpu ? static_cast<Renderer&>(*m_cpuRenderer)
                                                               : static_cast<Renderer&>(*m_gpuRenderer);
    m_stats = renderer.render(m_scene->draws(), frame);
    auto const& output = renderer.outputTexture();

    // Rasterization is already complete; this pass only tone-maps the chosen HDR texture.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    m_displayProgram->use();
    output.bind(m_displayProgram->getTextureBinding("uHdrColor"));

    m_fullscreenVao.bind();
    gfx::RHI::draw(gfx::RHI::Primitive::Triangles, 0, 3);
    gfx::VertexArray::unbind();

    output.unbind();

    // render UI elements
    renderUi();
}

void SoftwareRendererApp::renderUi() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuiWindowFlags const flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Rasterizer", nullptr, flags)) {
        int mode = static_cast<int>(m_renderBackend);
        ImGui::RadioButton("CPU", &mode, static_cast<int>(RenderBackend::Cpu));
        ImGui::SameLine();
        ImGui::RadioButton("GPU fragment", &mode, static_cast<int>(RenderBackend::Gpu));
        m_renderBackend = static_cast<RenderBackend>(mode);
        ImGui::Separator();
        ImGui::Text(m_renderBackend == RenderBackend::Gpu ? "source triangles: %u" : "triangles: %u",
                    m_stats.triangleCount);
        ImGui::Text(m_renderBackend == RenderBackend::Gpu ? "CPU submit: %.3f ms" : "geometry time: %.3f ms",
                    m_stats.preparationMilliseconds);
        ImGui::Text(m_renderBackend == RenderBackend::Gpu ? "GPU time: %.3f ms" : "raster time: %.3f ms",
                    m_stats.milliseconds);
        ImGui::ColorEdit3("light color", &m_directionalLight.color.x);
        ImGui::SliderFloat("light intensity", &m_directionalLight.intensity, 0.0f, 20.0f);
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
