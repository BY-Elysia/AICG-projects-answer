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

    Mesh const& mesh() const {
        return m_mesh;
    }

private:
    Mesh m_mesh;
    PbrMaterial m_material;

private:
    void load(std::string const& filePath);

    void uploadTextures();
};
