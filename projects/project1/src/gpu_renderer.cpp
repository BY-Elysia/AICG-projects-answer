#include "gpu_renderer.h"
#include "shader_utils.h"
#include "texture_utils.h"
#include "tile_binning.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <common/timer.h>
#include <graphics/rhi.h>

namespace {
void setSampler(gfx::GLProgram const& program, char const* name, int unit) {
    program.setUniform(glGetUniformLocation(program.getNativeHandle(), name), unit);
}
} // namespace

GpuRenderer::GpuRenderer(std::string const& vertexShader,
                         std::string const& visibilityShader,
                         std::string const& shadingShader) {
    m_visibilityProgram = makeDisplayProgram(vertexShader, visibilityShader);
    m_shadingProgram = makeDisplayProgram(vertexShader, shadingShader);
    setSampler(*m_visibilityProgram, "uTriangles", 0);
    setSampler(*m_visibilityProgram, "uTileRanges", 1);
    setSampler(*m_visibilityProgram, "uTileIndices", 2);
    setSampler(*m_shadingProgram, "uTriangles", 0);
    setSampler(*m_shadingProgram, "uOwner", 1);
    for (int i = 0; i < 5; ++i) {
        std::string const flag = "uHasTexture[" + std::to_string(i) + "]";
        std::string const sampler = "uTextures[" + std::to_string(i) + "]";
        m_hasTextureLocations[i] = glGetUniformLocation(m_shadingProgram->getNativeHandle(), flag.c_str());
        setSampler(*m_shadingProgram, sampler.c_str(), 2 + i);
    }
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
    glGenQueries(1, &m_timeQuery);
}

GpuRenderer::~GpuRenderer() {
    if (m_timeQuery) {
        glDeleteQueries(1, &m_timeQuery);
    }
}

void GpuRenderer::resizeBackend() {
    m_ownerTexture = makeRasterTexture(
        m_extent, gfx::Texture::Format::R32UI, gfx::Texture::ExternalFormat::RedInt, gfx::Texture::PixelType::UInt);
    m_visibilityFramebuffer = std::make_unique<gfx::Framebuffer>();
    m_visibilityFramebuffer->attachAsColor(0, *m_ownerTexture);
    m_visibilityFramebuffer->setColorWrite(0);
    m_shadingFramebuffer = std::make_unique<gfx::Framebuffer>();
    m_shadingFramebuffer->attachAsColor(0, *m_outputTexture);
    m_shadingFramebuffer->setColorWrite(0);
    if (!m_visibilityFramebuffer->isComplete() || !m_shadingFramebuffer->isComplete()) {
        throw std::runtime_error("Rasterizer visibility/HDR framebuffer is incomplete");
    }
}

void GpuRenderer::uploadPacked(std::unique_ptr<gfx::Texture2D>& texture,
                               size_t texelCount,
                               gfx::Texture::Format format,
                               gfx::Texture::ExternalFormat pixelFormat,
                               gfx::Texture::PixelType type,
                               size_t texelBytes,
                               void const* data) {
    // 2D textures avoid depending on a large GL_MAX_TEXTURE_BUFFER_SIZE, which GL 4.1
    // does not guarantee. Padding permits uploading the final partially filled row.
    size_t const width = std::min(size_t{1024}, static_cast<size_t>(m_maxTextureSize));
    size_t const height = std::max(size_t{1}, (texelCount + width - 1) / width);
    if (height > static_cast<size_t>(m_maxTextureSize)) {
        throw std::runtime_error("Rasterizer input exceeds the OpenGL 2D texture size limit");
    }
    std::vector<uint8_t> padded(width * height * texelBytes, 0);
    if (texelCount != 0) {
        std::memcpy(padded.data(), data, texelCount * texelBytes);
    }
    if (!texture || texture->getWidth() != width || texture->getHeight() != height) {
        texture = makeRasterTexture({width, height}, format, pixelFormat, type);
    }
    texture->update(padded.data(), pixelFormat, type);
}

RendererStats GpuRenderer::render(Mesh const& mesh, PbrMaterial const& material, FrameState const& frame) {
    resize(frame.extent);
    Timer geometryTimer;
    geometryTimer.tick();
    auto const triangles = prepareTriangles(mesh, frame);
    float const preparationMs = static_cast<float>(geometryTimer.tock() * 1000.0);

    if (triangles.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Too many rasterizer triangles");
    }
    Timer timer;
    timer.tick();
    RasterTiles const tiles = binTriangles(triangles, m_extent, TileSize);
    float const binningMs = static_cast<float>(timer.tock() * 1000.0);
    uploadPacked(m_triangleTexture,
                 triangles.size() * 14,
                 gfx::Texture::Format::RGBA32F,
                 gfx::Texture::ExternalFormat::RGBA,
                 gfx::Texture::PixelType::Float,
                 sizeof(glm::vec4),
                 triangles.data());
    uploadPacked(m_tileIndexTexture,
                 tiles.indices.size(),
                 gfx::Texture::Format::R32UI,
                 gfx::Texture::ExternalFormat::RedInt,
                 gfx::Texture::PixelType::UInt,
                 sizeof(uint32_t),
                 tiles.indices.data());
    if (!m_tileRangeTexture || m_tileRangeTexture->getWidth() != tiles.extent.x ||
        m_tileRangeTexture->getHeight() != tiles.extent.y) {
        m_tileRangeTexture = makeRasterTexture(tiles.extent,
                                               gfx::Texture::Format::RG32UI,
                                               gfx::Texture::ExternalFormat::RGInt,
                                               gfx::Texture::PixelType::UInt);
    }
    m_tileRangeTexture->update(tiles.ranges.data(), gfx::Texture::ExternalFormat::RGInt, gfx::Texture::PixelType::UInt);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, m_extent.x, m_extent.y);
    glBeginQuery(GL_TIME_ELAPSED, m_timeQuery);
    m_visibilityFramebuffer->bind();
    m_visibilityProgram->use();
    m_visibilityProgram->setUniform("uTileSize", TileSize);
    m_triangleTexture->bindTextureUnit(0);
    m_tileRangeTexture->bindTextureUnit(1);
    m_tileIndexTexture->bindTextureUnit(2);
    m_fullscreenVao.bind();
    gfx::RHI::draw(gfx::RHI::Primitive::Triangles, 0, 3);

    // The owner texture is sampled only while a different framebuffer is bound. Ordinary
    // framebuffer writes and later texture reads are ordered; no image memory barrier is needed.
    m_shadingFramebuffer->bind();
    m_shadingProgram->use();
    m_triangleTexture->bindTextureUnit(0);
    m_ownerTexture->bindTextureUnit(1);
    for (int i = 0; i < 5; ++i) {
        if (material.hasTexture[i]) {
            material.textures[i].gpuTexture->bindTextureUnit(2 + i);
        }
        else {
            glActiveTexture(GL_TEXTURE0 + 2 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        m_shadingProgram->setUniform(m_hasTextureLocations[i], material.hasTexture[i]);
    }
    m_shadingProgram->setUniform("uTriangleCount", static_cast<int>(triangles.size()));
    m_shadingProgram->setUniform("uCameraPosition", frame.cameraPosition);
    m_shadingProgram->setUniform("uLightDirection", frame.directionalLight.transform.getFront());
    m_shadingProgram->setUniform("uLightColor", frame.directionalLight.color);
    m_shadingProgram->setUniform("uLightIntensity", frame.directionalLight.intensity);
    m_shadingProgram->setUniform("uBaseColorFactor", material.baseColorFactor);
    m_shadingProgram->setUniform("uEmissiveFactor", material.emissiveFactor);
    m_shadingProgram->setUniform("uMetallicFactor", material.metallicFactor);
    m_shadingProgram->setUniform("uRoughnessFactor", material.roughnessFactor);
    m_shadingProgram->setUniform("uOcclusionStrength", material.occlusionStrength);
    gfx::RHI::draw(gfx::RHI::Primitive::Triangles, 0, 3);
    gfx::VertexArray::unbind();
    gfx::Framebuffer::unbind();
    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(m_timeQuery, GL_QUERY_RESULT, &nanoseconds);
    return {
        static_cast<float>(nanoseconds) / 1'000'000.0f,
        static_cast<uint32_t>(triangles.size()),
        binningMs,
        preparationMs
    };
}
