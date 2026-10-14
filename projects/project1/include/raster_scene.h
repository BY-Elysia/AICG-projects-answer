#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "raster_types.h"

class RasterScene {
public:
    explicit RasterScene(std::string const& filePath);

    ~RasterScene();

    RasterScene(RasterScene const&) = delete;

    RasterScene& operator=(RasterScene const&) = delete;

    PbrMaterial const& material() const {
        return m_material;
    }

    std::vector<PreparedTriangle> prepare(FrameState const& frame) const;

private:
    std::vector<RasterVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    PbrMaterial m_material;

private:
    void load(std::string const& filePath);

    void uploadTextures();
};
