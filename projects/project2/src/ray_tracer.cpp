#include "ray_tracer.h"
#include "bvh.h"
#include "random.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include <common/event/event_dispatcher.h>
#include <common/event/keyboard_event.h>
#include <glm/ext.hpp>
#include <graphics/rhi.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static std::string readTextFile(std::filesystem::path const& path) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Unable to open shader: " + path.string());
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

static gfx::GLProgram makeProgram(std::filesystem::path const& vertexPath, std::filesystem::path const& fragmentPath) {
    std::vector<gfx::ShaderModule> modules;
    modules.emplace_back(readTextFile(vertexPath), gfx::ShaderModule::Stage::Vertex);
    modules.emplace_back(readTextFile(fragmentPath), gfx::ShaderModule::Stage::Fragment);

    return gfx::GLProgram{modules};
}

RayTracer::RayTracer(Config const& config)
    : Application{config} {
    m_rayTracingProgram = std::make_unique<gfx::GLProgram>(
        makeProgram(getAssetPath("shaders/quad.vert"), getAssetPath("shaders/raytracing.frag")));

    m_screenQuad = std::make_unique<gfx::VertexArray>();
    m_nearestSampler = std::make_unique<gfx::Sampler>(gfx::Sampler::Description{
        .addressModeU = gfx::Sampler::AddressMode::ClampToEdge,
        .addressModeV = gfx::Sampler::AddressMode::ClampToEdge,
        .addressModeW = gfx::Sampler::AddressMode::ClampToEdge
    });
    m_skyboxSampler = std::make_unique<gfx::Sampler>(gfx::Sampler::Description{
        .minFilter = gfx::Sampler::Filter::Linear,
        .magFilter = gfx::Sampler::Filter::Linear,
        .addressModeU = gfx::Sampler::AddressMode::ClampToEdge,
        .addressModeV = gfx::Sampler::AddressMode::ClampToEdge,
        .addressModeW = gfx::Sampler::AddressMode::ClampToEdge
    });

    m_presentProgram = std::make_unique<gfx::GLProgram>(
        makeProgram(getAssetPath("shaders/quad.vert"), getAssetPath("shaders/quad.frag")));

    // camera
    m_camera = std::make_unique<PerspectiveCamera>(glm::radians(60.0f), m_window.aspectRatio(), 0.1f, 1000.0f);
    m_lucy = std::make_unique<Model>(getAssetPath("models/lucy.obj"));
    createSpheres();
    createSkybox();
    createTextures();
    createRenderScene(m_renderSceneIndex);
    initUI();

    resetClock();
}

RayTracer::~RayTracer() {
    if (m_uiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void RayTracer::handleEvent(Event& event) {
    EventDispatcher dispatcher{event};
    dispatcher.dispatch<KeyPressEvent>([this](KeyPressEvent& key) {
        if (key.getKeyCode() == KeyCode::Escape) {
            close();
            return true;
        }
        return false;
    });
}

void RayTracer::renderFrame() {
    if (m_activeSceneIndex != m_renderSceneIndex) {
        createRenderScene(m_renderSceneIndex);
    }

    updateCamera();
    auto const w = m_window.framebufferWidth(), h = m_window.framebufferHeight();
    if (w == 0 || h == 0) {
        return;
    }

    if (m_outFrames[0]->getWidth() != w || m_outFrames[0]->getHeight() != h) {
        createTextures();
        resetAccumulation();
    }
    m_camera->aspect = m_window.aspectRatio();

    gfx::RHI::setViewport({w, h});
    gfx::RHI::enableDepthTest(false);
    gfx::RHI::enableBlend(false);
    gfx::RHI::enableScissorTest(false);
    gfx::RHI::enableCullFace(false);
    auto const view = m_camera->getViewMatrix();
    auto const projection = m_camera->getProjectionMatrix();
    auto const screenToRaster =
        glm::scale(glm::mat4{1}, glm::vec3{.5f * w, .5f * h, 1}) * glm::translate(glm::mat4{1}, glm::vec3{1, 1, 0});
    m_rayTracingProgram->use();
    m_sampleFramebuffers[m_currentWriteBuffer]->bind();
    m_rayTracingProgram->setUniform("totalSamples", m_sampleCount);
    m_rayTracingProgram->setUniform("camera.cameraToWorld", glm::inverse(view));
    m_rayTracingProgram->setUniform("camera.rasterToCamera", glm::inverse(projection) * glm::inverse(screenToRaster));
    bindSceneTextures();
    drawScreenQuad();
    gfx::Framebuffer::unbind();

    m_presentProgram->use();
    auto const frameUnit = m_presentProgram->getTextureBinding("frame");
    m_outFrames[m_currentWriteBuffer]->bindTextureUnit(frameUnit);
    m_nearestSampler->bind(frameUnit);
    drawScreenQuad();

    ++m_sampleCount;
    std::swap(m_currentReadBuffer, m_currentWriteBuffer);

    renderUI();
}

void RayTracer::updateCamera() {
    auto const step = 5.0f * getDeltaTime();
    auto const forward = m_camera->transform.getFront();
    auto const right = m_camera->transform.getRight();
    bool moved{};
    if (m_input.isKeyDown(KeyCode::W)) {
        m_camera->transform.position += forward * step;
        moved = true;
    }
    if (m_input.isKeyDown(KeyCode::S)) {
        m_camera->transform.position -= forward * step;
        moved = true;
    }
    if (m_input.isKeyDown(KeyCode::A)) {
        m_camera->transform.position -= right * step;
        moved = true;
    }
    if (m_input.isKeyDown(KeyCode::D)) {
        m_camera->transform.position += right * step;
        moved = true;
    }
    if (m_input.isKeyDown(KeyCode::Q)) {
        m_camera->transform.position += m_camera->transform.getUp() * step;
        moved = true;
    }
    if (m_input.isKeyDown(KeyCode::E)) {
        m_camera->transform.position -= m_camera->transform.getUp() * step;
        moved = true;
    }
    if (moved) {
        resetAccumulation();
    }
}

void RayTracer::createSpheres() {
    m_spheres.emplace_back(glm::vec3{0.0f, -2000.0f, 0.0f}, 2000.0f);
    m_sphereMaterials.emplace_back(Material::Type::Lambertian, 1.0f, 0.0f, glm::vec3{0.5f});

    constexpr float maxRadius = 15.0f;
    constexpr float pi = 3.141592654f;
    constexpr int radiusSegments = 100;
    constexpr int angleSegments = 60;
    for (int i = 0; i <= radiusSegments; i += 3) {
        for (int j = 0; j <= angleSegments; j += 2) {
            auto const rho = maxRadius * i / radiusSegments;
            auto const theta = 2 * pi * j / angleSegments + 0.2f * randomFloat();
            auto const y = randomFloat(0.08f, 0.25f);
            glm::vec3 const center{rho * std::cos(theta), y, rho * std::sin(theta)};

            if (glm::length(center - glm::vec3{0.0f, 0.2f, 0.0f}) <= 2.0f ||
                glm::length(center - glm::vec3{4.0f, 0.2f, -2.0f}) <= 2.0f ||
                glm::length(center - glm::vec3{-4.0f, 0.2f, 2.0f}) <= 2.0f ||
                glm::length(center - glm::vec3{4.0f, 0.0f, 5.0f}) <= 1.0f) {
                continue;
            }

            Material material;
            auto const chooseMat = randomFloat();
            if (chooseMat < 0.4f) {
                material = {Material::Type::Lambertian, 1.0f, 0.0f, randomVec3() * randomVec3()};
            }
            else if (chooseMat < 0.95f) {
                material.type = Material::Type::Metal;
                material.ior = 1.0f;
                material.fuzz = randomFloat(0.0f, 0.5f);
                material.albedo = randomVec3(0.5f, 1.0f);
            }
            else {
                material = {Material::Type::Dielectric, 1.5f, 0.0f, glm::vec3{1.0f}};
            }

            m_spheres.emplace_back(center, center.y);
            m_sphereMaterials.push_back(material);
        }
    }

    m_spheres.emplace_back(glm::vec3{3.7f, 1.2f, 4.7f}, 1.2f);
    m_sphereMaterials.emplace_back(Material::Type::Dielectric, 1.5f, 0.0f, glm::vec3{1.0f});
    m_spheres.emplace_back(glm::vec3{-8.0f, 1.0f, -2.0f}, 1.0f);
    m_sphereMaterials.emplace_back(Material::Type::Lambertian, 1.0f, 0.0f, glm::vec3{0.2f, 0.4f, 0.8f});
    m_spheres.emplace_back(glm::vec3{3.0f, 3.0f, -8.0f}, 2.4f);
    m_sphereMaterials.emplace_back(Material::Type::Metal, 1.0f, 0.0f, glm::vec3{0.7f, 0.6f, 0.5f});
}

void RayTracer::createSkybox() {
    static constexpr std::array<char const*, 6> paths{
        "skyboxrt/right.jpg",
        "skyboxrt/left.jpg",
        "skyboxrt/top.jpg",
        "skyboxrt/bottom.jpg",
        "skyboxrt/front.jpg",
        "skyboxrt/back.jpg",
    };

    std::array<unsigned char*, 6> pixels{};
    int width{};
    int height{};
    for (size_t i = 0; i < paths.size(); ++i) {
        int faceWidth{};
        int faceHeight{};
        int faceChannels{};
        auto const path = getAssetPath(paths[i]);
        pixels[i] = stbi_load(path.string().c_str(), &faceWidth, &faceHeight, &faceChannels, STBI_rgb);
        if (pixels[i] == nullptr) {
            for (auto* pixel : pixels) {
                stbi_image_free(pixel);
            }
            throw std::runtime_error("Unable to load skybox texture: " + path.string());
        }

        if (i == 0) {
            width = faceWidth;
            height = faceHeight;
        }
        else if (faceWidth != width || faceHeight != height) {
            for (auto* pixel : pixels) {
                stbi_image_free(pixel);
            }
            throw std::runtime_error("Skybox faces must use the same dimensions");
        }
    }

    std::array<void const*, 6> data{};
    for (size_t i = 0; i < pixels.size(); ++i) {
        data[i] = pixels[i];
    }

    m_skybox = std::make_unique<gfx::TextureCubemap>(
        gfx::TextureCubemap::Specification{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .format = gfx::Texture::Format::RGB8,
            .pixelFormat = gfx::Texture::ExternalFormat::RGB,
            .pixelType = gfx::Texture::PixelType::UByte
        },
        data);

    for (auto* pixel : pixels) {
        stbi_image_free(pixel);
    }
}

void RayTracer::createTextures() {
    auto const w = m_window.framebufferWidth();
    auto const h = m_window.framebufferHeight();
    std::vector<uint32_t> rng(w * h);
    for (uint32_t i = 0; i < rng.size(); ++i) {
        rng[i] = 1664525u * i + 1013904223u;
    }
    std::vector<glm::vec4> emptyFrame(w * h, glm::vec4{0.0f});

    for (size_t i = 0; i < 2; ++i) {
        m_sampleFramebuffers[i] = std::make_unique<gfx::Framebuffer>();
        m_outFrames[i] = std::make_unique<gfx::Texture2D>(
            gfx::Texture2D::Specification{
                .width = w,
                .height = h,
                .format = gfx::Texture::Format::RGBA32F,
                .pixelFormat = gfx::Texture::ExternalFormat::RGBA,
                .pixelType = gfx::Texture::PixelType::Float
            },
            emptyFrame.data());
        m_rngStates[i] = std::make_unique<gfx::Texture2D>(
            gfx::Texture2D::Specification{
                .width = w,
                .height = h,
                .format = gfx::Texture::Format::R32UI,
                .pixelFormat = gfx::Texture::ExternalFormat::RedInt,
                .pixelType = gfx::Texture::PixelType::UInt
            },
            rng.data());
        m_sampleFramebuffers[i]->attachAsColor(0, *m_outFrames[i]);
        m_sampleFramebuffers[i]->attachAsColor(1, *m_rngStates[i]);
        m_sampleFramebuffers[i]->setColorWrites({0, 1});

        if (!m_sampleFramebuffers[i]->isComplete()) {
            throw std::runtime_error(m_sampleFramebuffers[i]->getDiagnostic());
        }
    }

    m_currentReadBuffer = 0;
    m_currentWriteBuffer = 1;
}

void RayTracer::createRenderScene(int index) {
    switch (index) {
    case 0:
        createScene1();
        break;
    case 1:
        createScene2();
        break;
    case 2:
        createScene3();
        break;
    default:
        throw std::invalid_argument("Invalid ray-tracing scene index");
    }

    m_activeSceneIndex = index;
    resetAccumulation();
}

void RayTracer::createScene1() {
    m_camera->transform.position = {0.0f, 0.0f, 12.0f};
    m_camera->transform.lookAt(glm::vec3{0.0f});

    createPrimitiveBuffer({{{0.0f, 0.0f, 0.0f}, 1.5f}, {{4.0f, 0.0f, 0.0f}, 1.5f}, {{-4.0f, 0.0f, 0.0f}, 1.5f}},
                          {},
                          {},
                          {{Material::Type::Dielectric, 1.5f, 0.0f, {1.0f, 1.0f, 1.0f}},
                           {Material::Type::Metal, 1.0f, 0.0f, {0.7f, 0.6f, 0.5f}},
                           {Material::Type::Lambertian, 1.0f, 0.0f, {0.8f, 0.4f, 0.2f}}},
                          {});
}

void RayTracer::createScene2() {
    m_camera->transform.position = {12.48607f, 1.925702f, 1.317638f};
    m_camera->transform.lookAt({0.0f, 3.0f, 0.0f});

    createPrimitiveBuffer(m_spheres, {}, {}, m_sphereMaterials, {});
}

void RayTracer::createScene3() {
    m_camera->transform.position = {12.48607f, 1.925702f, 1.317638f};
    m_camera->transform.lookAt({0.0f, 3.0f, 0.0f});

    auto const scale = glm::scale(glm::mat4{1.0f}, glm::vec3{0.6f});
    auto const rotation = glm::rotate(glm::mat4{1.0f}, glm::radians(90.0f), glm::vec3{0.0f, 1.0f, 0.0f});
    createPrimitiveBuffer(m_spheres,
                          {m_lucy.get(), m_lucy.get(), m_lucy.get()},
                          {rotation * scale,
                           glm::translate(glm::mat4{1.0f}, glm::vec3{-4.0f, 0.0f, 2.0f}) * rotation * scale,
                           glm::translate(glm::mat4{1.0f}, glm::vec3{4.0f, 0.0f, -2.0f}) * rotation * scale},
                          m_sphereMaterials,
                          {{Material::Type::Dielectric, 1.5f, 0.0f, {1.0f, 1.0f, 1.0f}},
                           {Material::Type::Metal, 1.0f, 0.0f, {0.7f, 0.6f, 0.5f}},
                           {Material::Type::Lambertian, 1.0f, 0.0f, {0.8f, 0.4f, 0.2f}}});
}

template <typename T> static uint32_t padBuffer(std::vector<T>& data, uint32_t components) {
    constexpr size_t width = 2048;
    auto const rowSize = width * components;
    auto const height = std::max(size_t{1}, (data.size() + rowSize - 1) / rowSize);
    data.resize(height * rowSize, T{});
    return static_cast<uint32_t>(height);
}

void RayTracer::createPrimitiveBuffer(std::vector<Sphere> const& spheres,
                                      std::vector<Model const*> const& models,
                                      std::vector<glm::mat4> const& transforms,
                                      std::vector<Material> const& sphereMaterials,
                                      std::vector<Material> const& modelMaterials) {
    if (spheres.size() != sphereMaterials.size() || models.size() != transforms.size() ||
        models.size() != modelMaterials.size()) {
        throw std::invalid_argument("Scene shapes, transforms and materials do not match");
    }

    size_t totalVertices = 0;
    size_t totalTriangles = 0;
    for (auto const* model : models) {
        totalVertices += model->vertices().size();
        totalTriangles += model->triangles().size();
    }

    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::vector<Primitive> scenePrimitives;
    vertices.reserve(totalVertices);
    triangles.reserve(totalTriangles);
    scenePrimitives.reserve(spheres.size() + totalTriangles);

    std::vector<float> sphereData;
    sphereData.reserve(spheres.size() * 4);
    for (size_t i = 0; i < spheres.size(); ++i) {
        auto const& sphere = spheres[i];
        sphereData.insert(sphereData.end(), {sphere.position.x, sphere.position.y, sphere.position.z, sphere.radius});
        scenePrimitives.push_back({Primitive::Type::Sphere, static_cast<uint32_t>(i), static_cast<uint32_t>(i)});
    }
    auto const sphereHeight = padBuffer(sphereData, 4);
    m_sphereBuffer = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = kBufferWidth,
            .height = sphereHeight,
            .format = gfx::Texture::Format::RGBA32F,
            .pixelFormat = gfx::Texture::ExternalFormat::RGBA,
            .pixelType = gfx::Texture::PixelType::Float
        },
        sphereData.data());

    for (size_t i = 0; i < models.size(); ++i) {
        auto const vertexOffset = static_cast<uint32_t>(vertices.size());
        auto const normalTransform = glm::mat3(glm::transpose(glm::inverse(transforms[i])));
        for (auto const& vertex : models[i]->vertices()) {
            vertices.push_back(
                {glm::vec3(transforms[i] * glm::vec4(vertex.position, 1.0f)),
                 normalTransform * vertex.normal,
                 vertex.texCoord});
        }

        for (auto const& triangle : models[i]->triangles()) {
            scenePrimitives.push_back(
                {Primitive::Type::Triangle,
                 static_cast<uint32_t>(triangles.size()),
                 static_cast<uint32_t>(sphereMaterials.size() + i)});
            triangles.push_back(
                {{triangle.v[0] + vertexOffset, triangle.v[1] + vertexOffset, triangle.v[2] + vertexOffset}});
        }
    }

    // Explicit component packing matches getTriangleData in the reference shader,
    // regardless of GLM's CPU struct alignment.
    std::vector<float> vertexData;
    vertexData.reserve(vertices.size() * 8);
    for (auto const& vertex : vertices) {
        vertexData.insert(vertexData.end(),
                          {vertex.position.x,
                           vertex.position.y,
                           vertex.position.z,
                           vertex.normal.x,
                           vertex.normal.y,
                           vertex.normal.z,
                           vertex.texCoord.x,
                           vertex.texCoord.y});
    }
    auto const vertexHeight = padBuffer(vertexData, 4);
    m_vertexBuffer = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = kBufferWidth,
            .height = vertexHeight,
            .format = gfx::Texture::Format::RGBA32F,
            .pixelFormat = gfx::Texture::ExternalFormat::RGBA,
            .pixelType = gfx::Texture::PixelType::Float
        },
        vertexData.data());

    std::vector<int32_t> triangleData;
    triangleData.reserve(triangles.size() * 3);
    for (auto const& triangle : triangles) {
        for (auto index : triangle.v) {
            triangleData.push_back(static_cast<int32_t>(index));
        }
    }
    auto const triangleHeight = padBuffer(triangleData, 3);
    m_indexBuffer = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = kBufferWidth,
            .height = triangleHeight,
            .format = gfx::Texture::Format::RGB32I,
            .pixelFormat = gfx::Texture::ExternalFormat::RGBInt,
            .pixelType = gfx::Texture::PixelType::Int
        },
        triangleData.data());

    std::vector<float> materialData;
    materialData.reserve((sphereMaterials.size() + modelMaterials.size()) * 6);
    auto appendMaterial = [&](Material const& material) {
        materialData.insert(materialData.end(),
                            {static_cast<float>(material.type),
                             material.ior,
                             material.fuzz,
                             material.albedo.x,
                             material.albedo.y,
                             material.albedo.z});
    };
    for (auto const& material : sphereMaterials) {
        appendMaterial(material);
    }
    for (auto const& material : modelMaterials) {
        appendMaterial(material);
    }
    auto const materialHeight = padBuffer(materialData, 3);
    m_materialBuffer = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = kBufferWidth,
            .height = materialHeight,
            .format = gfx::Texture::Format::RGB32F,
            .pixelFormat = gfx::Texture::ExternalFormat::RGB,
            .pixelType = gfx::Texture::PixelType::Float
        },
        materialData.data());

    BVH bvh{spheres, scenePrimitives, triangles, vertices};
    std::vector<int32_t> primitiveData;
    primitiveData.reserve(scenePrimitives.size() * 3);
    for (auto index : bvh.orderedPrimitiveIndices()) {
        auto const& primitive = scenePrimitives[index];
        primitiveData.insert(primitiveData.end(),
                             {static_cast<int32_t>(primitive.type),
                              static_cast<int32_t>(primitive.shapeIndex),
                              static_cast<int32_t>(primitive.materialIndex)});
    }
    auto const primitiveHeight = padBuffer(primitiveData, 3);
    m_primitiveBuffer = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = kBufferWidth,
            .height = primitiveHeight,
            .format = gfx::Texture::Format::RGB32I,
            .pixelFormat = gfx::Texture::ExternalFormat::RGBInt,
            .pixelType = gfx::Texture::PixelType::Int
        },
        primitiveData.data());

    std::vector<float> bvhData;
    bvhData.reserve(bvh.nodes().size() * 9);
    for (auto const& node : bvh.nodes()) {
        bvhData.insert(bvhData.end(),
                       {node.bounds.min.x,
                        node.bounds.min.y,
                        node.bounds.min.z,
                        node.bounds.max.x,
                        node.bounds.max.y,
                        node.bounds.max.z,
                        static_cast<float>(node.type),
                        static_cast<float>(node.first),
                        static_cast<float>(node.second)});
    }
    auto const bvhHeight = padBuffer(bvhData, 3);
    m_bvhBuffer = std::make_unique<gfx::Texture2D>(
        gfx::Texture2D::Specification{
            .width = kBufferWidth,
            .height = bvhHeight,
            .format = gfx::Texture::Format::RGB32F,
            .pixelFormat = gfx::Texture::ExternalFormat::RGB,
            .pixelType = gfx::Texture::PixelType::Float
        },
        bvhData.data());

    m_rayTracingProgram->setUniform("nPrimitives", static_cast<int>(scenePrimitives.size()));
    m_rayTracingProgram->setUniform("nBVHNodes", static_cast<int>(bvh.nodes().size()));

    std::cout << "Scene Statistics\n"
              << "+ Spheres: " << spheres.size() << '\n'
              << "+ Models:  " << models.size() << '\n'
              << "  + vertices:  " << totalVertices << '\n'
              << "  + triangles: " << totalTriangles << '\n'
              << "+ BVH nodes: " << bvh.nodes().size() << ", depth: " << bvh.height() << std::endl;
}

void RayTracer::bindSceneTextures() {
    auto bind = [this](gfx::Texture const& texture, char const* name, gfx::Sampler const& sampler) {
        auto const unit = m_rayTracingProgram->getTextureBinding(name);
        texture.bindTextureUnit(unit);
        sampler.bind(unit);
    };

    bind(*m_skybox, "sky", *m_skyboxSampler);
    bind(*m_sphereBuffer, "sphereBuffer", *m_nearestSampler);
    bind(*m_materialBuffer, "materialBuffer", *m_nearestSampler);
    bind(*m_primitiveBuffer, "primitiveBuffer", *m_nearestSampler);
    bind(*m_outFrames[m_currentReadBuffer], "RTResult", *m_nearestSampler);
    bind(*m_rngStates[m_currentReadBuffer], "oldRngState", *m_nearestSampler);
    bind(*m_indexBuffer, "triangleIndexBuffer", *m_nearestSampler);
    bind(*m_vertexBuffer, "vertexBuffer", *m_nearestSampler);
    bind(*m_bvhBuffer, "bvh", *m_nearestSampler);
}

void RayTracer::drawScreenQuad() const {
    m_screenQuad->bind();
    gfx::RHI::draw(gfx::RHI::Primitive::Triangles, 0, 3);
    gfx::VertexArray::unbind();
}

void RayTracer::initUI() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    auto* window = static_cast<GLFWwindow*>(m_window.handle());
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("Unable to initialize ImGui GLFW backend");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 410 core")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("Unable to initialize ImGui OpenGL backend");
    }
    m_uiInitialized = true;
}

void RayTracer::renderUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2{0.0f, 0.0f}, ImGuiCond_Once);
    auto const flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Control Panel", nullptr, flags)) {
        ImGui::Text("switch scenes");
        ImGui::Separator();
        static char const* scenes[]{"scene 1", "scene 2", "scene 3"};
        ImGui::Combo("##1", &m_renderSceneIndex, scenes, IM_ARRAYSIZE(scenes));
        ImGui::NewLine();
        ImGui::Text("statistics");
        ImGui::Separator();
        ImGui::Text("samples: %u", m_sampleCount);
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
