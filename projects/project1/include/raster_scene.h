#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "raster_types.h"

class RasterScene {
public:
    explicit RasterScene(const std::string& filePath);

    ~RasterScene();

    RasterScene(const RasterScene&) = delete;

    RasterScene& operator=(const RasterScene&) = delete;

    const PbrMaterial& material() const {
        return m_material;
    }

    std::vector<PreparedTriangle> prepare(const FrameState& frame) const;

private:
    std::vector<RasterVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    PbrMaterial m_material;

private:
    void load(const std::string& filePath);

    void uploadTextures();
};
