#pragma once

#include <vector>
#include "raster_types.h"

struct RasterTiles {
    glm::ivec2 extent;
    // Each tile stores (first index, count) in one shared, variable-length index array.
    std::vector<glm::uvec2> ranges;
    std::vector<uint32_t> indices;
};

RasterTiles binTriangles(std::vector<PreparedTriangle> const& triangles,
                         glm::ivec2 extent, int tileSize);
