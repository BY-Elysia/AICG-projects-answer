#define GLM_ENABLE_EXPERIMENTAL
#include "tile_binning.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <glm/gtx/exterior_product.hpp>

RasterTiles binTriangles(std::vector<PreparedTriangle> const& triangles, glm::ivec2 extent, int tileSize) {
    if (extent.x <= 0 || extent.y <= 0 || tileSize <= 0) {
        throw std::invalid_argument("Invalid tile binning extent");
    }
    RasterTiles tiles;
    tiles.extent = {(extent.x + tileSize - 1) / tileSize, (extent.y + tileSize - 1) / tileSize};
    tiles.ranges.resize(size_t(tiles.extent.x) * tiles.extent.y, glm::uvec2{0});
    auto visitTiles = [&](PreparedTriangle const& triangle, auto visit) {
        glm::vec2 const a{triangle.vertices[0].screenAndInvW};
        glm::vec2 const b{triangle.vertices[1].screenAndInvW};
        glm::vec2 const c{triangle.vertices[2].screenAndInvW};
        if (glm::cross(b - a, c - a) <= 0) {
            return;
        }
        auto const minimum = glm::max(glm::ivec2{glm::floor(glm::min(a, glm::min(b, c)))}, glm::ivec2{0});
        auto const maximum = glm::min(glm::ivec2{glm::ceil(glm::max(a, glm::max(b, c)))}, extent - 1);
        if (minimum.x > maximum.x || minimum.y > maximum.y) {
            return;
        }
        auto const first = minimum / tileSize, last = maximum / tileSize;
        for (int y = first.y; y <= last.y; ++y) {
            for (int x = first.x; x <= last.x; ++x) {
                visit(size_t(y) * tiles.extent.x + x);
            }
        }
    };
    for (auto const& triangle : triangles) {
        visitTiles(triangle, [&](size_t tile) { ++tiles.ranges[tile].y; });
    }
    size_t total = 0;
    for (auto& range : tiles.ranges) {
        range.x = static_cast<uint32_t>(total);
        total += range.y;
        if (total > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Tile references exceed 32-bit indexing capacity");
        }
    }
    tiles.indices.resize(total);
    std::vector<uint32_t> cursors;
    cursors.reserve(tiles.ranges.size());
    for (auto const& range : tiles.ranges) {
        cursors.push_back(range.x);
    }
    // Fill in submission order: equal-depth triangles choose the same first owner as the CPU.
    for (size_t index = 0; index < triangles.size(); ++index) {
        visitTiles(triangles[index],
                   [&](size_t tile) { tiles.indices[cursors[tile]++] = static_cast<uint32_t>(index); });
    }
    return tiles;
}
