#include "software_renderer_app.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <glad/glad.h>
#include <glm/ext.hpp>

namespace {
std::vector<glm::vec4> readOutput(gfx::Texture2D const& texture) {
    std::vector<glm::vec4> pixels(size_t(texture.getWidth()) * texture.getHeight());
    texture.bindTextureUnit(0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
    texture.unbindTextureUnit(0);
    return pixels;
}

float displayValue(float hdr) {
    return std::pow(std::max(hdr, 0.0f) / (std::max(hdr, 0.0f) + 1.0f), 1.0f / 2.2f);
}

void compareOutputs(std::string const& label,
                    std::vector<glm::vec4> const& cpu,
                    std::vector<glm::vec4> const& gpu,
                    bool expectGeometry) {
    glm::vec3 const background{0.025f, 0.03f, 0.04f};
    size_t covered = 0, coverageMismatch = 0, largeDifferences = 0;
    double squaredError = 0;
    for (size_t pixel = 0; pixel < cpu.size(); ++pixel) {
        bool cpuCovered = false, gpuCovered = false;
        float largestDifference = 0;
        for (int channel = 0; channel < 4; ++channel) {
            if (!std::isfinite(cpu[pixel][channel]) || !std::isfinite(gpu[pixel][channel])) {
                throw std::runtime_error(label + ": non-finite output");
            }
            if (channel == 3) {
                if (cpu[pixel].a != 1.0f || gpu[pixel].a != 1.0f) {
                    throw std::runtime_error(label + ": invalid output alpha");
                }
                continue;
            }
            cpuCovered |= std::abs(cpu[pixel][channel] - background[channel]) > 0.0005f;
            gpuCovered |= std::abs(gpu[pixel][channel] - background[channel]) > 0.0005f;
            float const error = std::abs(displayValue(cpu[pixel][channel]) - displayValue(gpu[pixel][channel]));
            squaredError += double(error) * error;
            largestDifference = std::max(largestDifference, error);
        }
        covered += cpuCovered;
        coverageMismatch += cpuCovered != gpuCovered;
        largeDifferences += largestDifference > 0.03f;
    }
    double const rms = std::sqrt(squaredError / (cpu.size() * 3));
    std::cout << label << ": covered=" << covered << ", coverage mismatch=" << coverageMismatch
              << ", display RMS=" << rms << ", large differences=" << largeDifferences << '\n';
    // A few silhouette pixels may differ due to floating-point edge evaluation. Compare
    // display-space colors to avoid allowing the HDR highlights to dominate the metric.
    if ((expectGeometry && covered < 100) || (!expectGeometry && covered != 0) ||
        coverageMismatch > std::max(size_t{4}, cpu.size() / 1000) || rms > 0.005 ||
        largeDifferences > std::max(size_t{4}, cpu.size() / 100)) {
        throw std::runtime_error(label + ": CPU/GPU comparison failed");
    }
}

struct TestTriangle {
    float depth;
    glm::vec3 normal;
};

// Raw mesh inputs: the renderer must transform, assemble and clip these triangles itself.
Mesh testMesh(std::vector<TestTriangle> const& triangles) {
    Mesh mesh;
    auto& vertices = mesh.editVertices();
    auto& indices = mesh.editIndices();
    glm::vec2 const positions[]{{10, 10}, {110, 10}, {10, 90}};
    for (auto const& triangle : triangles) {
        for (auto position : positions) {
            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({glm::vec3{position, -triangle.depth}, triangle.normal, glm::vec2{0}});
        }
    }
    return mesh;
}
} // namespace

void SoftwareRendererApp::validateBackends() {
    auto checkDraws = [this](std::string const& label,
                             FrameState const& frame,
                             std::span<DrawItem const> draws,
                             bool expectGeometry,
                             std::optional<uint32_t> expectedTriangles = std::nullopt) {
        auto const cpuStats = m_cpuRenderer->render(draws, frame);
        auto const cpu = readOutput(m_cpuRenderer->outputTexture());
        auto const gpuStats = m_gpuRenderer->render(draws, frame);
        auto const gpu = readOutput(m_gpuRenderer->outputTexture());
        uint32_t sourceTriangles = 0;
        for (DrawItem const& draw : draws) {
            sourceTriangles += static_cast<uint32_t>(draw.mesh->indices().size() / 3);
        }
        if (gpuStats.triangleCount != sourceTriangles ||
            (expectedTriangles && cpuStats.triangleCount != *expectedTriangles)) {
            throw std::runtime_error(label + ": triangle statistics differ");
        }
        compareOutputs(label, cpu, gpu, expectGeometry);
        if (glGetError() != GL_NO_ERROR) {
            throw std::runtime_error(label + ": OpenGL error");
        }
        return cpu;
    };
    auto check = [&](std::string const& label,
                     FrameState const& frame,
                     Mesh const& mesh,
                     PbrMaterial const& material,
                     bool expectGeometry,
                     std::optional<uint32_t> expectedTriangles = std::nullopt,
                     glm::mat4 const& transform = glm::mat4{1}) {
        DrawItem const draw{&mesh, &material, transform};
        return checkDraws(label, frame, std::span{&draw, 1}, expectGeometry, expectedTriangles);
    };

    FrameState frame;
    frame.view = m_camera->getViewMatrix();
    frame.cameraPosition = m_camera->transform.position;
    frame.directionalLight = m_directionalLight;
    glm::ivec2 const extents[]{{640, 360}, {319, 181}};
    for (auto extent : extents) {
        frame.extent = extent;
        frame.projection = glm::perspective(glm::radians(45.0f), float(extent.x) / extent.y, 0.1f, 100.0f);
        for (float angle : {0.0f, 0.7f}) {
            frame.model = glm::rotate(glm::mat4{1}, angle, glm::vec3{0, 1, 0});
            checkDraws("helmet " + std::to_string(extent.x) + " angle " + std::to_string(angle),
                       frame,
                       m_scene->draws(),
                       true);
        }
    }
    if (m_scene->draws().size() != 1 || m_scene->draws()[0].mesh->gpuUploadCount() != 1 ||
        m_scene->draws()[0].mesh->gpuIndexUploadCount() != 1) {
        throw std::runtime_error("Static mesh buffers were uploaded more than once");
    }
    // Exercise shared clipping with a model intersecting the side planes.
    frame.model = glm::translate(glm::mat4{1}, glm::vec3{1.7f, 0, 0});
    checkDraws("clipped helmet", frame, m_scene->draws(), true);

    frame.extent = {127, 97};
    frame.model = glm::mat4{1};
    frame.view = glm::mat4{1};
    frame.projection = glm::ortho(0.0f, float(frame.extent.x), 0.0f, float(frame.extent.y), 0.0f, 1.0f);
    frame.cameraPosition = {0, 0, 5};
    PbrMaterial simple;
    simple.metallicFactor = 0;
    simple.roughnessFactor = 0.8f;
    RasterScene hierarchy(getAssetPath("models/HierarchyTest.gltf").string());
    auto const hierarchyDraws = hierarchy.draws();
    if (hierarchyDraws.size() != 4 || hierarchyDraws[0].mesh == hierarchyDraws[1].mesh ||
        hierarchyDraws[2].mesh != hierarchyDraws[3].mesh || hierarchyDraws[0].material == hierarchyDraws[1].material ||
        hierarchyDraws[1].material == hierarchyDraws[2].material ||
        glm::vec3(hierarchyDraws[0].transform[3]) != glm::vec3(5, 0, 0) ||
        glm::vec3(hierarchyDraws[1].transform[3]) != glm::vec3(5, 0, 0) ||
        glm::vec3(hierarchyDraws[2].transform[3]) != glm::vec3(5, 5, 0.1f) ||
        glm::vec3(hierarchyDraws[3].transform[3]) != glm::vec3(-5, 0, 0.1f)) {
        throw std::runtime_error("Hierarchical glTF draw list is incorrect");
    }
    auto const hierarchyPixels = checkDraws("hierarchical glTF", frame, hierarchyDraws, true, 4);
    for (DrawItem const& draw : hierarchyDraws) {
        if (draw.mesh->gpuUploadCount() != 1 || draw.mesh->gpuIndexUploadCount() != 1) {
            throw std::runtime_error("Shared scene mesh buffers were uploaded more than once");
        }
    }
    std::array<size_t, 3> dominantColors{};
    for (glm::vec4 const& pixel : hierarchyPixels) {
        for (int channel = 0; channel < 3; ++channel) {
            if (pixel[channel] > pixel[(channel + 1) % 3] * 2.0f && pixel[channel] > pixel[(channel + 2) % 3] * 2.0f) {
                ++dominantColors[channel];
            }
        }
    }
    if (std::any_of(dominantColors.begin(), dominantColors.end(), [](size_t count) { return count < 10; })) {
        throw std::runtime_error("Hierarchical glTF did not display all three materials");
    }
    std::vector<TestTriangle> triangles{{0.8f, {0, 1, 0}}, {0.2f, {0, 0, 1}}};
    auto const depthForward = check("overlapping depth", frame, testMesh(triangles), simple, true, 2);
    std::reverse(triangles.begin(), triangles.end());
    auto const depthReverse = check("reversed depth order", frame, testMesh(triangles), simple, true, 2);
    if (depthForward != depthReverse) {
        throw std::runtime_error("Visibility depends on submission order for different depths");
    }
    Mesh reindexed = testMesh({{0.8f, {0, 1, 0}}, {0.2f, {0, 0, 1}}});
    auto const originalIndices = check("cached indices", frame, reindexed, simple, true, 2);
    reindexed.editIndices() = {0, 1, 2, 3, 5, 4};
    auto const updatedIndices = check("updated indices", frame, reindexed, simple, true, 1);
    if (originalIndices == updatedIndices || reindexed.gpuUploadCount() != 1 || reindexed.gpuIndexUploadCount() != 2) {
        throw std::runtime_error("Modified mesh indices were not used or reuploaded");
    }
    triangles = {{0.2f, {0, 1, 0}}, {0.2f, {0, 0, 1}}};
    auto const tieForward = check("equal depth first owner", frame, testMesh(triangles), simple, true, 2);
    std::reverse(triangles.begin(), triangles.end());
    auto const tieReverse = check("equal depth reversed owner", frame, testMesh(triangles), simple, true, 2);
    if (tieForward == tieReverse) {
        throw std::runtime_error("Equal-depth test did not distinguish the two materials");
    }
    // Put the visible triangle after more than 1024 references in the same tile. The old
    // fixed-capacity GPU list discarded it; variable-length lists must retain every reference.
    triangles.assign(1300, {0.8f, {0, 1, 0}});
    triangles.push_back({0.2f, {0, 0, 1}});
    auto const crowded = check("1301 triangles per tile", frame, testMesh(triangles), simple, true, 1301);
    if (crowded != depthForward) {
        throw std::runtime_error("Crowded tile did not retain its nearest triangle");
    }
    // Feedback must retain vertices outside the clip volume so our own clipper can
    // construct new triangles before fixed-function clipping.
    Mesh nearPlane = testMesh({{0.2f, {0, 0, 1}}});
    nearPlane.editVertices().front().position.z = 0.2f;
    check("near-plane clipping", frame, nearPlane, simple, true, 2);
    for (auto& vertex : nearPlane.editVertices()) {
        vertex.position.z = 0.2f;
    }
    check("fully clipped mesh", frame, nearPlane, simple, false, 0);
    if (nearPlane.gpuUploadCount() != 2) {
        throw std::runtime_error("Modified mesh vertices were not reuploaded");
    }
    FrameState perspectiveFrame = frame;
    perspectiveFrame.view = glm::mat4{1};
    perspectiveFrame.projection =
        glm::perspective(glm::radians(70.0f), float(frame.extent.x) / frame.extent.y, 0.1f, 10.0f);
    perspectiveFrame.cameraPosition = {0, 0, 0};
    Mesh crossingCamera;
    crossingCamera.editVertices() = {
        {{-0.5f, -0.5f, -1.0f}, {0, 0, 1}, {0, 0}},
        {{0.5f, -0.5f, -1.0f}, {0, 0, 1}, {1, 0}},
        {{0.0f, 0.5f, 0.2f}, {0, 0, 1}, {0.5f, 1}},
    };
    crossingCamera.editIndices() = {0, 1, 2};
    check("triangle crossing camera plane", perspectiveFrame, crossingCamera, simple, true);
    checkDraws("empty scene", frame, {}, false, 0);

    // The scene keeps local vertices. Node transforms must be applied inside each renderer.
    Mesh transformed = testMesh({{0.2f, {0, 1, 0}}});
    glm::mat4 const nodeMatrix = glm::translate(glm::mat4{1}, glm::vec3{12, 2, 0}) *
                                 glm::rotate(glm::mat4{1}, 0.2f, glm::vec3{0, 0, 1}) *
                                 glm::scale(glm::mat4{1}, glm::vec3{0.8f, 0.9f, 1});
    auto const nodeTransform = check("mesh node transform", frame, transformed, simple, true, 1, nodeMatrix);
    FrameState instanceFrame = frame;
    instanceFrame.model = nodeMatrix;
    auto const instanceTransform = check("equivalent instance transform", instanceFrame, transformed, simple, true, 1);
    compareOutputs("node/instance transform equivalence", nodeTransform, instanceTransform, true);

    for (Renderer* renderer :
         {static_cast<Renderer*>(m_cpuRenderer.get()), static_cast<Renderer*>(m_gpuRenderer.get())}) {
        bool rejected = false;
        try {
            FrameState invalid = frame;
            invalid.extent = {0, 97};
            renderer->render({}, invalid);
        }
        catch (std::invalid_argument const&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("Zero-sized raster target was accepted");
        }
        for (auto const& indices : {std::vector<uint32_t>{0, 1}, std::vector<uint32_t>{0, 1, 3}}) {
            Mesh invalid = testMesh({{0.2f, {0, 0, 1}}});
            invalid.editIndices() = indices;
            rejected = false;
            try {
                DrawItem const draw{&invalid, &simple};
                renderer->render(std::span{&draw, 1}, frame);
            }
            catch (std::invalid_argument const&) {
                rejected = true;
            }
            if (!rejected) {
                throw std::runtime_error("Invalid mesh indices were accepted");
            }
        }
    }
    // Also exercise presentation, UI and repeated switching at the real framebuffer extent.
    for (int i = 0; i < 4; ++i) {
        m_renderBackend = i % 2 == 0 ? RenderBackend::Cpu : RenderBackend::Gpu;
        m_directionalLight.color = i % 2 == 0 ? glm::vec3{1} : glm::vec3{0.5f, 0.8f, 1};
        m_directionalLight.intensity = 4.0f + i;
        m_window.pollEvents();
        renderFrame();
        m_window.swapBuffers();
        if (glGetError() != GL_NO_ERROR) {
            throw std::runtime_error("Presentation/UI OpenGL error");
        }
    }
    std::cout << "Renderer pipeline validation passed\n";
}
