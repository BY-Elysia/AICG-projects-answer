#include "gpu_renderer.h"
#include "shader_utils.h"
#include "texture_utils.h"
#include "tile_binning.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <common/timer.h>
#include <graphics/rhi.h>
#include <glm/ext.hpp>

namespace {
void setSampler(gfx::GLProgram const& program, char const* name, int unit) {
    program.setUniform(glGetUniformLocation(program.getNativeHandle(), name), unit);
}
} // namespace

GpuRenderer::GpuRenderer(std::string const& transformShader,
                         std::string const& fullscreenShader,
                         std::string const& visibilityShader,
                         std::string const& shadingShader) {
    m_transformProgram =
        makeTransformFeedbackProgram(transformShader, {"tfClip", "tfWorldPosition", "tfNormal", "tfUv"});
    m_visibilityProgram = makeDisplayProgram(fullscreenShader, visibilityShader);
    m_shadingProgram = makeDisplayProgram(fullscreenShader, shadingShader);
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
    glGenQueries(1, &m_vertexTimeQuery);
    glGenTransformFeedbacks(1, &m_transformFeedback);
}

GpuRenderer::~GpuRenderer() {
    if (m_transformFeedback) {
        glDeleteTransformFeedbacks(1, &m_transformFeedback);
    }
    if (m_vertexTimeQuery) {
        glDeleteQueries(1, &m_vertexTimeQuery);
    }
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

std::vector<TransformedVertex> GpuRenderer::transformVertices(Mesh const& mesh,
                                                              glm::mat4 const& transform,
                                                              FrameState const& frame,
                                                              float& milliseconds) {
    milliseconds = 0.0f;
    if (mesh.vertices.empty()) {
        return {};
    }
    if (mesh.vertices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::runtime_error("Too many vertices for the GPU vertex pass");
    }
    size_t const inputBytes = mesh.vertices.size() * sizeof(RasterVertex);
    size_t const outputBytes = mesh.vertices.size() * sizeof(TransformedVertex);
    if (!m_vertexBuffer) {
        m_vertexBuffer = std::make_unique<gfx::Buffer>(
            gfx::Buffer::UsageFlags::VertexBuffer, inputBytes, gfx::Buffer::Usage::DynamicDraw);
        m_vertexVao.setFloatAttribute(
            *m_vertexBuffer,
            {0, 3, gfx::VertexArray::DataType::Float32, sizeof(RasterVertex), offsetof(RasterVertex, position)});
        m_vertexVao.setFloatAttribute(
            *m_vertexBuffer,
            {1, 3, gfx::VertexArray::DataType::Float32, sizeof(RasterVertex), offsetof(RasterVertex, normal)});
        m_vertexVao.setFloatAttribute(
            *m_vertexBuffer,
            {2, 2, gfx::VertexArray::DataType::Float32, sizeof(RasterVertex), offsetof(RasterVertex, uv)});
    }
    else if (m_vertexBuffer->getSize() != inputBytes) {
        m_vertexBuffer->resize(inputBytes, gfx::Buffer::Usage::DynamicDraw);
    }
    m_vertexBuffer->write(mesh.vertices.data());
    if (!m_transformedBuffer) {
        m_transformedBuffer = std::make_unique<gfx::Buffer>(
            gfx::Buffer::UsageFlags::TransformFeedbackBuffer, outputBytes, gfx::Buffer::Usage::DynamicRead);
    }
    else if (m_transformedBuffer->getSize() != outputBytes) {
        m_transformedBuffer->resize(outputBytes, gfx::Buffer::Usage::DynamicRead);
    }

    m_transformProgram->use();
    m_transformProgram->setUniform("uMeshTransform", transform);
    m_transformProgram->setUniform("uModel", frame.model);
    m_transformProgram->setUniform("uModelViewProjection", frame.projection * frame.view * frame.model);
    m_transformProgram->setUniform("uNodeNormalTransform", glm::transpose(glm::inverse(glm::mat3(transform))));
    m_transformProgram->setUniform("uNormalTransform", glm::transpose(glm::inverse(glm::mat3(frame.model))));

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, m_transformFeedback);
    m_transformedBuffer->bindAsTransformFeedback(0);
    m_vertexVao.bind();
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginQuery(GL_TIME_ELAPSED, m_vertexTimeQuery);
    glBeginTransformFeedback(GL_POINTS);
    gfx::RHI::draw(gfx::RHI::Primitive::Points, 0, static_cast<uint32_t>(mesh.vertices.size()));
    glEndTransformFeedback();
    glEndQuery(GL_TIME_ELAPSED);
    glDisable(GL_RASTERIZER_DISCARD);
    gfx::Buffer::unbindAsTransformFeedback(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    gfx::VertexArray::unbind();

    // Reading synchronizes with the captured GPU writes. CPU clipping/binning need these
    // varyings; the shader is the sole source of GPU-backend transformed vertices.
    std::vector<TransformedVertex> vertices(mesh.vertices.size());
    m_transformedBuffer->read(vertices.data());
    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(m_vertexTimeQuery, GL_QUERY_RESULT, &nanoseconds);
    milliseconds = static_cast<float>(nanoseconds) / 1'000'000.0f;
    return vertices;
}

RendererStats GpuRenderer::render(std::span<DrawItem const> draws, FrameState const& frame) {
    resize(frame.extent);
    Timer geometryTimer;
    geometryTimer.tick();
    float vertexMs = 0.0f;
    std::vector<PreparedTriangle> triangles;
    struct MaterialRange {
        PbrMaterial const* material;
        uint32_t begin;
        uint32_t end;
    };
    std::vector<MaterialRange> ranges;
    for (DrawItem const& draw : draws) {
        if (!draw.mesh || !draw.material) {
            throw std::invalid_argument("Draw item needs a mesh and material");
        }
        float drawVertexMs = 0.0f;
        auto const vertices = transformVertices(*draw.mesh, draw.transform, frame, drawVertexMs);
        vertexMs += drawVertexMs;
        auto prepared = prepareTriangles(vertices, draw.mesh->indices, frame.extent);
        if (prepared.size() > static_cast<size_t>(std::numeric_limits<int>::max()) - triangles.size()) {
            throw std::runtime_error("Too many rasterizer triangles");
        }
        uint32_t const begin = static_cast<uint32_t>(triangles.size());
        triangles.insert(triangles.end(), prepared.begin(), prepared.end());
        if (!prepared.empty()) {
            ranges.push_back({draw.material, begin, static_cast<uint32_t>(triangles.size())});
        }
    }
    float const preparationMs = static_cast<float>(geometryTimer.tock() * 1000.0);
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
    constexpr GLfloat background[]{0.025f, 0.03f, 0.04f, 1.0f};
    glClearBufferfv(GL_COLOR, 0, background);
    m_shadingProgram->use();
    m_triangleTexture->bindTextureUnit(0);
    m_ownerTexture->bindTextureUnit(1);
    m_shadingProgram->setUniform("uTriangleCount", static_cast<int>(triangles.size()));
    m_shadingProgram->setUniform("uCameraPosition", frame.cameraPosition);
    m_shadingProgram->setUniform("uLightDirection", frame.directionalLight.transform.getFront());
    m_shadingProgram->setUniform("uLightColor", frame.directionalLight.color);
    m_shadingProgram->setUniform("uLightIntensity", frame.directionalLight.intensity);
    for (MaterialRange const& range : ranges) {
        PbrMaterial const& material = *range.material;
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
        m_shadingProgram->setUniform("uTriangleBegin", range.begin);
        m_shadingProgram->setUniform("uTriangleEnd", range.end);
        m_shadingProgram->setUniform("uBaseColorFactor", material.baseColorFactor);
        m_shadingProgram->setUniform("uEmissiveFactor", material.emissiveFactor);
        m_shadingProgram->setUniform("uMetallicFactor", material.metallicFactor);
        m_shadingProgram->setUniform("uRoughnessFactor", material.roughnessFactor);
        m_shadingProgram->setUniform("uOcclusionStrength", material.occlusionStrength);
        gfx::RHI::draw(gfx::RHI::Primitive::Triangles, 0, 3);
    }
    gfx::VertexArray::unbind();
    gfx::Framebuffer::unbind();
    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(m_timeQuery, GL_QUERY_RESULT, &nanoseconds);
    return {
        static_cast<float>(nanoseconds) / 1'000'000.0f,
        static_cast<uint32_t>(triangles.size()),
        binningMs,
        preparationMs,
        vertexMs
    };
}
