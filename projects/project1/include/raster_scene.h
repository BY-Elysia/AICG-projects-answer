#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "raster_types.h"

class RasterScene {
public:
    explicit RasterScene(std::filesystem::path const& filePath);

    ~RasterScene() = default;

    RasterScene(RasterScene const&) = delete;

    RasterScene& operator=(RasterScene const&) = delete;

    std::vector<DrawItem> const& drawItems() const {
        return m_drawItems;
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
    std::vector<DrawItem> m_drawItems;

private:
    void load(std::filesystem::path const& filePath);

    void uploadTextures();
};
