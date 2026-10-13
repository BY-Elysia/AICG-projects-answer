#include "gpu_rasterizer.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

GpuRasterizer::GpuRasterizer(
    const std::string& binningShader, const std::string& visibilityShader,
    const std::string& shadingShader) {
    // Use separate compute programs so visibility is resolved before a later pass writes colors.
    m_binningProgram = makeComputeProgram(binningShader);
    m_visibilityProgram = makeComputeProgram(visibilityShader);
    m_shadingProgram = makeComputeProgram(shadingShader);
    // GLProgram reflects only the first array element; cache every boolean location explicitly.
    for (int i = 0; i < 5; ++i) {
        const std::string name = "uHasTexture[" + std::to_string(i) + "]";
        m_hasTextureLocations[i] = glGetUniformLocation(m_shadingProgram->getNativeHandle(), name.c_str());
    }

    glGenBuffers(1, &m_triangleBuffer);
    glGenBuffers(1, &m_tileCountBuffer);
    glGenBuffers(1, &m_tileIndexBuffer);
    glGenQueries(1, &m_timeQuery);
}

GpuRasterizer::~GpuRasterizer() {
    if (m_timeQuery) {
        glDeleteQueries(1, &m_timeQuery);
    }
    if (m_triangleBuffer) {
        glDeleteBuffers(1, &m_triangleBuffer);
    }
    if (m_tileCountBuffer) {
        glDeleteBuffers(1, &m_tileCountBuffer);
    }
    if (m_tileIndexBuffer) {
        glDeleteBuffers(1, &m_tileIndexBuffer);
    }
}

void GpuRasterizer::resizeBackend() {
    m_tileExtent = {
        (m_extent.x + TileSize - 1) / TileSize, (m_extent.y + TileSize - 1) / TileSize};
    // Allocate independent depth and primitive-owner attachments for the visibility buffer.
    m_depthTexture =
        makeRasterTexture(m_extent, gfx::Texture::Format::R32F, gfx::Texture::ExternalFormat::Red);
    m_ownerTexture = makeRasterTexture(m_extent, gfx::Texture::Format::R32UI,
                          gfx::Texture::ExternalFormat::RedInt, gfx::Texture::PixelType::UInt);

    const size_t tileCount = static_cast<size_t>(m_tileExtent.x) * m_tileExtent.y;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tileCountBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, tileCount * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tileIndexBuffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER, tileCount * MaxTrianglesPerTile * sizeof(GLuint), nullptr,
        GL_DYNAMIC_DRAW);
}

RasterizerStats GpuRasterizer::render(
    const std::vector<PreparedTriangle>& triangles, const PbrMaterial& material,
    const FrameState& frame) {
    // Upload triangles after the shared CPU frontend has transformed and clipped them.
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_triangleBuffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER, std::max(size_t{1}, triangles.size()) * sizeof(PreparedTriangle),
        triangles.empty() ? nullptr : triangles.data(),
        GL_DYNAMIC_DRAW);
    const float farDepth = 1.0f;
    constexpr GLuint noOwner = std::numeric_limits<GLuint>::max();
    glClearTexImage(m_depthTexture->getNativeHandle(), 0, GL_RED, GL_FLOAT, &farDepth);
    glClearTexImage(m_ownerTexture->getNativeHandle(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &noOwner);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tileCountBuffer);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    // Bind the triangle stream and tile buffers used by the binning and visibility passes.
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_triangleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_tileCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_tileIndexBuffer);
    glBindImageTexture(0, m_depthTexture->getNativeHandle(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
    glBindImageTexture(1, m_ownerTexture->getNativeHandle(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    for (int i = 0; i < 5; ++i) {
        if (material.hasTexture[i]) {
            material.textures[i].gpuTexture->bindTextureUnit(i);
        } else {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    glBeginQuery(GL_TIME_ELAPSED, m_timeQuery);
    m_binningProgram->use();
    m_binningProgram->setUniform("uTriangleCount", static_cast<int>(triangles.size()));
    m_binningProgram->setUniform("uExtent", glm::vec2(m_extent));
    m_binningProgram->setUniform("uTileExtent", glm::vec2(m_tileExtent));
    m_binningProgram->setUniform("uTileSize", TileSize);
    m_binningProgram->setUniform("uMaxTrianglesPerTile", MaxTrianglesPerTile);
    if (!triangles.empty()) {
        glDispatchCompute(static_cast<GLuint>(triangles.size()), 1, 1);
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    m_visibilityProgram->use();
    m_visibilityProgram->setUniform("uTriangleCount", static_cast<int>(triangles.size()));
    m_visibilityProgram->setUniform("uExtent", glm::vec2(m_extent));
    m_visibilityProgram->setUniform("uTileExtent", glm::vec2(m_tileExtent));
    m_visibilityProgram->setUniform("uTileSize", TileSize);
    m_visibilityProgram->setUniform("uMaxTrianglesPerTile", MaxTrianglesPerTile);
    // Each dispatched workgroup exclusively owns one tile, eliminating cross-workgroup locks.
    glDispatchCompute(m_tileExtent.x, m_tileExtent.y, 1);
    // The shading pass reads the owner image only after every visibility image write is complete.
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Pass 2: one invocation shades one pixel by reconstructing its winning triangle.
    m_shadingProgram->use();
    glBindImageTexture(0, m_outputTexture->getNativeHandle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(1, m_ownerTexture->getNativeHandle(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    m_shadingProgram->setUniform("uTriangleCount", static_cast<int>(triangles.size()));
    m_shadingProgram->setUniform("uExtent", glm::vec2(m_extent));
    m_shadingProgram->setUniform("uCameraPosition", frame.cameraPosition);
    m_shadingProgram->setUniform(
        "uLightDirection", frame.directionalLight.transform.getFront());
    m_shadingProgram->setUniform("uLightColor", frame.directionalLight.color);
    m_shadingProgram->setUniform("uLightIntensity", frame.directionalLight.intensity);
    m_shadingProgram->setUniform("uBaseColorFactor", material.baseColorFactor);
    m_shadingProgram->setUniform("uEmissiveFactor", material.emissiveFactor);
    m_shadingProgram->setUniform("uMetallicFactor", material.metallicFactor);
    m_shadingProgram->setUniform("uRoughnessFactor", material.roughnessFactor);
    m_shadingProgram->setUniform("uOcclusionStrength", material.occlusionStrength);
    for (int i = 0; i < 5; ++i) {
        m_shadingProgram->setUniform(m_hasTextureLocations[i], material.hasTexture[i]);
    }
    glDispatchCompute((m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1);
    // Make imageStore writes visible before the display pass samples the output texture.
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    glEndQuery(GL_TIME_ELAPSED);

    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(m_timeQuery, GL_QUERY_RESULT, &nanoseconds);
    return {
        static_cast<float>(nanoseconds) / 1'000'000.0f, static_cast<uint32_t>(triangles.size())};
}
