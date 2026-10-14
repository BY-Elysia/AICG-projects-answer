#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "raster_types.h"

class RasterScene {
public:
    explicit RasterScene(std::string const& filePath);

    ~RasterScene();

    RasterScene(RasterScene const&) = delete;

    RasterScene& operator=(RasterScene const&) = delete;

    std::span<DrawItem const> draws() const {
        return m_draws;
    }

private:
    struct Node {
        glm::mat4 localTransform{1.0f};
        int mesh = -1;
        std::vector<Node> children;
    };

    std::vector<Mesh> m_meshes;
    std::vector<PbrMaterial> m_materials;
    std::vector<Node> m_roots;
    std::vector<DrawItem> m_draws;

private:
    void load(std::string const& filePath);

    void uploadTextures();
};
