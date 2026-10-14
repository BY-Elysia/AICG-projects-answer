#pragma once

#include <memory>
#include <span>
#include <vector>

#include "raster_types.h"

// Owns the complete geometry-to-HDR pipeline. Scene supplies drawable meshes and materials.
class Renderer {
public:
    virtual ~Renderer() = default;

    // Process all draw items together so depth and material ownership span the whole scene.
    virtual RendererStats render(std::span<DrawItem const> draws, FrameState const& frame) = 0;

    // SoftwareRendererApp presents the HDR result separately from the rendering pipeline.
    gfx::Texture2D const& outputTexture() const {
        return *m_outputTexture;
    }

protected:
    glm::ivec2 m_extent{0};
    // CPU uploads HDR pixels; GPU writes the same format through a framebuffer attachment.
    std::unique_ptr<gfx::Texture2D> m_outputTexture;

    virtual void resizeBackend() = 0;

    // CPU geometry helpers; prepared triangles never cross the backend entry point.
    void resize(glm::ivec2 const& extent);
    static std::vector<PreparedTriangle>
    prepareTriangles(Mesh const& mesh, glm::mat4 const& transform, FrameState const& frame);
    static std::vector<PreparedTriangle>
    prepareTriangles(std::span<TransformedVertex const> vertices, std::span<uint32_t const> indices, glm::ivec2 extent);
};
