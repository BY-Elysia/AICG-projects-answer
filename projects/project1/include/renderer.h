#pragma once

#include <memory>
#include <vector>

#include "raster_types.h"

// Owns the complete geometry-to-HDR pipeline. Scene supplies original mesh and material data.
class Renderer {
public:
    virtual ~Renderer() = default;

    // Each backend receives original geometry and transforms, and owns its full pipeline.
    virtual RendererStats render(Mesh const& mesh, PbrMaterial const& material, FrameState const& frame) = 0;

    // SoftwareRendererApp presents the HDR result separately from the rendering pipeline.
    gfx::Texture2D const& outputTexture() const {
        return *m_outputTexture;
    }

protected:
    glm::ivec2 m_extent{0};
    // CPU uploads HDR pixels; GPU writes the same format through a framebuffer attachment.
    std::unique_ptr<gfx::Texture2D> m_outputTexture;

    virtual void resizeBackend() = 0;

    // Shared implementation helpers; prepared triangles never cross the backend entry point.
    void resize(glm::ivec2 const& extent);
    static std::vector<PreparedTriangle> prepareTriangles(Mesh const& mesh, FrameState const& frame);
};
