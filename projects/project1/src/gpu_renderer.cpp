#include "gpu_renderer.h"
#include "shader_utils.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>
#include <common/timer.h>
#include <graphics/rhi.h>
#include <glm/ext.hpp>

namespace {
void setSampler(gfx::GLProgram const& program, char const* name, int unit) {
    program.setUniform(glGetUniformLocation(program.getNativeHandle(), name), unit);
}
} // namespace

GpuRenderer::GpuRenderer(std::string const& transformShader,
                         std::string const& rasterVertexShader,
                         std::string const& rasterGeometryShader,
                         std::string const& rasterFragmentShader) {
    m_transformProgram = makeTransformFeedbackProgram(transformShader,
                                                      gfx::GLProgram::TransformFeedbackBufferMode::Interleaved,
                                                      {"tfClip", "tfWorldPosition", "tfNormal", "tfUv"});
    std::vector<gfx::ShaderModule> modules;
    modules.push_back(loadShader(rasterVertexShader, gfx::ShaderModule::Stage::Vertex));
    modules.push_back(loadShader(rasterGeometryShader, gfx::ShaderModule::Stage::Geometry));
    modules.push_back(loadShader(rasterFragmentShader, gfx::ShaderModule::Stage::Fragment));
    m_rasterProgram = std::make_unique<gfx::GLProgram>(modules);
    for (int i = 0; i < 5; ++i) {
        std::string const flag = "uHasTexture[" + std::to_string(i) + "]";
        std::string const sampler = "uTextures[" + std::to_string(i) + "]";
        m_hasTextureLocations[i] = glGetUniformLocation(m_rasterProgram->getNativeHandle(), flag.c_str());
        setSampler(*m_rasterProgram, sampler.c_str(), i);
    }
    glGenQueries(static_cast<GLsizei>(m_timeQueries.size()), m_timeQueries.data());
}

GpuRenderer::~GpuRenderer() {
    glDeleteQueries(static_cast<GLsizei>(m_timeQueries.size()), m_timeQueries.data());
}

void GpuRenderer::resizeBackend() {
    m_depthBuffer = std::make_unique<gfx::Renderbuffer>(m_extent.x, m_extent.y, gfx::Renderbuffer::Format::Depth32F);
    m_framebuffer = std::make_unique<gfx::Framebuffer>();
    m_framebuffer->attachAsColor(0, *m_outputTexture);
    m_framebuffer->attachAsDepth(*m_depthBuffer);
    m_framebuffer->setColorWrite(0);
    if (!m_framebuffer->isComplete()) {
        throw std::runtime_error("GPU rasterizer framebuffer is incomplete");
    }
}

void GpuRenderer::transformVertices(Mesh const& mesh, glm::mat4 const& transform, FrameState const& frame) {
    auto const& indices = mesh.indices();
    size_t const outputBytes = indices.size() * sizeof(TransformedVertex);
    auto const& inputVao = mesh.gpuVertexArray();
    if (!m_transformedBuffer) {
        m_transformedBuffer = std::make_unique<gfx::Buffer>(
            gfx::Buffer::UsageFlags::TransformFeedbackBuffer, outputBytes, gfx::Buffer::Usage::DynamicDraw);
        for (uint32_t attribute = 0; attribute < 4; ++attribute) {
            m_feedbackVao.setFloatAttribute(*m_transformedBuffer,
                                            {attribute,
                                             4,
                                             gfx::VertexArray::DataType::Float32,
                                             sizeof(TransformedVertex),
                                             attribute * sizeof(glm::vec4)});
        }
    }
    else if (m_transformedBuffer->getSize() != outputBytes) {
        m_transformedBuffer->resize(outputBytes, gfx::Buffer::Usage::DynamicDraw);
    }

    m_transformProgram->use();
    m_transformProgram->setUniform("uMeshTransform", transform);
    m_transformProgram->setUniform("uModel", frame.model);
    m_transformProgram->setUniform("uModelViewProjection", frame.projection * frame.view * frame.model);
    m_transformProgram->setUniform("uNodeNormalTransform", glm::transpose(glm::inverse(glm::mat3(transform))));
    m_transformProgram->setUniform("uNormalTransform", glm::transpose(glm::inverse(glm::mat3(frame.model))));

    m_transformFeedback.setBuffer(0, *m_transformedBuffer);
    inputVao.bind();
    glEnable(GL_RASTERIZER_DISCARD);
    m_transformFeedback.begin(gfx::RHI::Primitive::Triangles);
    gfx::RHI::drawIndexed(gfx::RHI::Primitive::Triangles, 0, static_cast<uint32_t>(indices.size()));
    gfx::TransformFeedback::end();
    glDisable(GL_RASTERIZER_DISCARD);
    gfx::TransformFeedback::unbind();
    gfx::VertexArray::unbind();
}

void GpuRenderer::setMaterial(PbrMaterial const& material) {
    for (int i = 0; i < 5; ++i) {
        if (material.hasTexture[i]) {
            material.textures[i].gpuTexture->bindTextureUnit(i);
        }
        else {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        m_rasterProgram->setUniform(m_hasTextureLocations[i], material.hasTexture[i]);
    }
    m_rasterProgram->setUniform("uBaseColorFactor", material.baseColorFactor);
    m_rasterProgram->setUniform("uEmissiveFactor", material.emissiveFactor);
    m_rasterProgram->setUniform("uMetallicFactor", material.metallicFactor);
    m_rasterProgram->setUniform("uRoughnessFactor", material.roughnessFactor);
    m_rasterProgram->setUniform("uOcclusionStrength", material.occlusionStrength);
}

RendererStats GpuRenderer::render(std::span<DrawItem const> draws, FrameState const& frame) {
    resize(frame.extent);
    Timer timer;
    timer.tick();
    uint32_t sourceTriangleCount = 0;
    for (DrawItem const& draw : draws) {
        if (!draw.mesh || !draw.material) {
            throw std::invalid_argument("Draw item needs a mesh and material");
        }
        auto const& indices = draw.mesh->indices();
        if (indices.size() % 3 != 0) {
            throw std::invalid_argument("Mesh indices must describe complete triangles");
        }
        if (indices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max())) {
            throw std::runtime_error("Too many indices for the GPU vertex pass");
        }
        for (uint32_t index : indices) {
            if (index >= draw.mesh->vertices().size()) {
                throw std::invalid_argument("Mesh index references a missing vertex");
            }
        }
        if (indices.size() / 3 > std::numeric_limits<uint32_t>::max() - sourceTriangleCount) {
            throw std::runtime_error("Too many source triangles");
        }
        sourceTriangleCount += static_cast<uint32_t>(indices.size() / 3);
    }
    for (size_t i = 0; i < m_timeQueries.size(); ++i) {
        if (!m_queryPending[i]) {
            continue;
        }
        GLint ready = GL_FALSE;
        glGetQueryObjectiv(m_timeQueries[i], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (ready) {
            GLuint64 nanoseconds = 0;
            glGetQueryObjectui64v(m_timeQueries[i], GL_QUERY_RESULT, &nanoseconds);
            m_lastGpuMilliseconds = static_cast<float>(nanoseconds) / 1'000'000.0f;
            m_queryPending[i] = false;
        }
    }
    int queryIndex = -1;
    for (size_t i = 0; i < m_timeQueries.size(); ++i) {
        if (!m_queryPending[i]) {
            queryIndex = static_cast<int>(i);
            break;
        }
    }

    m_framebuffer->bind();
    glViewport(0, 0, m_extent.x, m_extent.y);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    constexpr GLfloat background[]{0.025f, 0.03f, 0.04f, 1.0f};
    constexpr GLfloat farDepth = 1.0f;
    if (queryIndex >= 0) {
        glBeginQuery(GL_TIME_ELAPSED, m_timeQueries[queryIndex]);
    }
    glClearBufferfv(GL_COLOR, 0, background);
    glClearBufferfv(GL_DEPTH, 0, &farDepth);

    glm::mat4 const viewProjection = frame.projection * frame.view;
    glm::mat4 const inverseViewProjection = glm::inverse(viewProjection);
    for (DrawItem const& draw : draws) {
        if (draw.mesh->indices().empty()) {
            continue;
        }
        transformVertices(*draw.mesh, draw.transform, frame);

        m_rasterProgram->use();
        m_rasterProgram->setUniform("uExtent", glm::vec2(frame.extent));
        m_rasterProgram->setUniform("uViewProjection", viewProjection);
        m_rasterProgram->setUniform("uInverseViewProjection", inverseViewProjection);
        m_rasterProgram->setUniform("uCameraPosition", frame.cameraPosition);
        m_rasterProgram->setUniform("uLightDirection", frame.directionalLight.transform.getFront());
        m_rasterProgram->setUniform("uLightColor", frame.directionalLight.color);
        m_rasterProgram->setUniform("uLightIntensity", frame.directionalLight.intensity);
        setMaterial(*draw.material);
        m_feedbackVao.bind();
        m_transformFeedback.draw(gfx::RHI::Primitive::Triangles);
        gfx::VertexArray::unbind();
    }
    gfx::Framebuffer::unbind();
    glDisable(GL_DEPTH_TEST);
    if (queryIndex >= 0) {
        glEndQuery(GL_TIME_ELAPSED);
        m_queryPending[queryIndex] = true;
    }
    return {m_lastGpuMilliseconds, sourceTriangleCount, static_cast<float>(timer.tock() * 1000.0)};
}
