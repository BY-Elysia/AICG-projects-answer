#include "gpu_renderer.h"
#include "shader_utils.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>
#include <common/timer.h>
#include <graphics/rhi.h>
#include <glm/ext.hpp>

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
        m_hasTextureLocations[i] = m_rasterProgram->getUniformVarLocation(flag);
        m_rasterProgram->setUniform(sampler, i);
    }
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
    gfx::RHI::enableRasterizerDiscard(true);
    m_transformFeedback.begin(gfx::RHI::Primitive::Triangles);
    gfx::RHI::drawIndexed(gfx::RHI::Primitive::Triangles, 0, static_cast<uint32_t>(indices.size()));
    gfx::TransformFeedback::end();
    gfx::RHI::enableRasterizerDiscard(false);
    gfx::TransformFeedback::unbind();
    gfx::VertexArray::unbind();
}

void GpuRenderer::setMaterial(PbrMaterial const& material) {
    for (int i = 0; i < 5; ++i) {
        if (material.hasTexture[i]) {
            material.textures[i].gpuTexture->bindTextureUnit(i);
        }
        else {
            gfx::Texture2D::unbindFromTextureUnit(i);
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
        if (indices.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
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
        if (m_timeQueries[i].available()) {
            m_lastGpuMilliseconds = m_timeQueries[i].getElapsedTime() * 1000.0f;
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
    gfx::RHI::setViewport({static_cast<uint32_t>(m_extent.x), static_cast<uint32_t>(m_extent.y)});
    gfx::RHI::enableBlend(false);
    gfx::RHI::enableCullFace(false);
    gfx::RHI::enableScissorTest(false);
    gfx::RHI::enableDepthTest(true);
    gfx::RHI::setDepthFunc(gfx::RHI::CompareFunc::Less);
    gfx::RHI::enableDepthWrite(true);
    gfx::RHI::setClearColor({0.025f, 0.03f, 0.04f, 1.0f});
    gfx::RHI::setClearDepth(1.0f);
    if (queryIndex >= 0) {
        m_timeQueries[queryIndex].tick();
    }
    gfx::RHI::clear(true, true);

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
    gfx::RHI::enableDepthTest(false);
    if (queryIndex >= 0) {
        m_timeQueries[queryIndex].tock();
        m_queryPending[queryIndex] = true;
    }
    return {m_lastGpuMilliseconds, sourceTriangleCount, static_cast<float>(timer.tock() * 1000.0)};
}
