#include "rasterizer_app.h"
#include "tile_binning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
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

PreparedTriangle testTriangle(float depth, glm::vec3 normal) {
    PreparedTriangle triangle;
    glm::vec2 const positions[]{{10, 10}, {110, 10}, {10, 90}};
    for (int corner = 0; corner < 3; ++corner) {
        triangle.vertices[corner].screenAndInvW = glm::vec4{positions[corner], depth, 1};
        triangle.vertices[corner].worldPosition = glm::vec4{0, 0, 0, 1};
        triangle.vertices[corner].normal = glm::vec4{normal, 0};
    }
    return triangle;
}
} // namespace

void RasterizerApp::validateBackends() {
    auto check = [this](std::string const& label,
                        FrameState const& frame,
                        std::vector<PreparedTriangle> const& triangles,
                        PbrMaterial const& material,
                        bool expectGeometry) {
        m_cpuRasterizer->resize(frame.extent);
        m_gpuRasterizer->resize(frame.extent);
        auto const cpuStats = m_cpuRasterizer->render(triangles, material, frame);
        auto const cpu = readOutput(m_cpuRasterizer->outputTexture());
        auto const gpuStats = m_gpuRasterizer->render(triangles, material, frame);
        auto const gpu = readOutput(m_gpuRasterizer->outputTexture());
        if (cpuStats.triangleCount != gpuStats.triangleCount || cpuStats.triangleCount != triangles.size()) {
            throw std::runtime_error(label + ": triangle statistics differ");
        }
        compareOutputs(label, cpu, gpu, expectGeometry);
        if (glGetError() != GL_NO_ERROR) {
            throw std::runtime_error(label + ": OpenGL error");
        }
        return cpu;
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
            check("helmet " + std::to_string(extent.x) + " angle " + std::to_string(angle),
                  frame,
                  m_scene->prepare(frame),
                  m_scene->material(),
                  true);
        }
    }
    // Exercise shared clipping with a model intersecting the side planes.
    frame.model = glm::translate(glm::mat4{1}, glm::vec3{1.7f, 0, 0});
    check("clipped helmet", frame, m_scene->prepare(frame), m_scene->material(), true);

    frame.extent = {127, 97};
    PbrMaterial simple;
    simple.metallicFactor = 0;
    simple.roughnessFactor = 0.8f;
    std::vector<PreparedTriangle> triangles{testTriangle(0.8f, {0, 1, 0}), testTriangle(0.2f, {0, 0, 1})};
    auto const depthForward = check("overlapping depth", frame, triangles, simple, true);
    std::reverse(triangles.begin(), triangles.end());
    auto const depthReverse = check("reversed depth order", frame, triangles, simple, true);
    if (depthForward != depthReverse) {
        throw std::runtime_error("Visibility depends on submission order for different depths");
    }
    triangles = {testTriangle(0.2f, {0, 1, 0}), testTriangle(0.2f, {0, 0, 1})};
    auto const tieForward = check("equal depth first owner", frame, triangles, simple, true);
    std::reverse(triangles.begin(), triangles.end());
    auto const tieReverse = check("equal depth reversed owner", frame, triangles, simple, true);
    if (tieForward == tieReverse) {
        throw std::runtime_error("Equal-depth test did not distinguish the two materials");
    }
    // Put the visible triangle after more than 1024 references in the same tile. The old
    // fixed-capacity GPU list discarded it; variable-length lists must retain every reference.
    triangles.assign(1300, testTriangle(0.8f, {0, 1, 0}));
    triangles.push_back(testTriangle(0.2f, {0, 0, 1}));
    auto const tiles = binTriangles(triangles, frame.extent, 16);
    if (tiles.ranges.front().y != triangles.size()) {
        throw std::runtime_error("Crowded tile lost triangle references");
    }
    auto const crowded = check("1301 triangles per tile", frame, triangles, simple, true);
    if (crowded != depthForward) {
        throw std::runtime_error("Crowded tile did not retain its nearest triangle");
    }
    check("empty scene", frame, {}, simple, false);

    for (Rasterizer* rasterizer :
         {static_cast<Rasterizer*>(m_cpuRasterizer.get()), static_cast<Rasterizer*>(m_gpuRasterizer.get())}) {
        bool rejected = false;
        try {
            rasterizer->resize({0, 97});
        }
        catch (std::invalid_argument const&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("Zero-sized raster target was accepted");
        }
    }
    // Also exercise presentation, UI and repeated switching at the real framebuffer extent.
    for (int i = 0; i < 4; ++i) {
        m_rasterBackend = i % 2 == 0 ? RasterBackend::Cpu : RasterBackend::Gpu;
        m_directionalLight.color = i % 2 == 0 ? glm::vec3{1} : glm::vec3{0.5f, 0.8f, 1};
        m_directionalLight.intensity = 4.0f + i;
        m_window.pollEvents();
        renderFrame();
        m_window.swapBuffers();
        if (glGetError() != GL_NO_ERROR) {
            throw std::runtime_error("Presentation/UI OpenGL error");
        }
    }
    std::cout << "Rasterizer validation passed\n";
}
