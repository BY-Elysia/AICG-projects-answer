#pragma once

#include <memory>
#include <vector>

#include "raster_types.h"

// Owns the complete geometry-to-HDR pipeline. Scene supplies original mesh and material data.
class Renderer {
public:
    virtual ~Renderer() = default;

    // Shared vertex processing and clipping feed the selected rasterization/shading backend.
    RendererStats render(Mesh const& mesh, PbrMaterial const& material, FrameState const& frame);

    // SoftwareRendererApp presents the HDR result separately from the rendering pipeline.
    gfx::Texture2D const& outputTexture() const {
        return *m_outputTexture;
    }

protected:
    glm::ivec2 m_extent{0};
    // CPU uploads HDR pixels; GPU writes the same format through a framebuffer attachment.
    std::unique_ptr<gfx::Texture2D> m_outputTexture;

    virtual void resizeBackend() = 0;
    virtual RendererStats renderPrepared(std::vector<PreparedTriangle> const& triangles,
                                         PbrMaterial const& material,
                                         FrameState const& frame) = 0;

private:
    void resize(glm::ivec2 const& extent);
    static std::vector<PreparedTriangle> prepareTriangles(Mesh const& mesh, FrameState const& frame);
};
