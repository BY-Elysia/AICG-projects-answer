#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include <graphics/texture/texture2d.h>

#include "bvh.h"
#include "material.h"

class RayTracer {
public:
    RayTracer(uint32_t width, uint32_t height);

    struct Scene {
        std::vector<Sphere> spheres;
        std::vector<Primitive> primitives;
        std::vector<Triangle> triangles;
        std::vector<Vertex> vertices;
        std::vector<Material> materials;
        std::vector<BVH::Node> nodes;
    };

    struct Skybox {
        uint32_t width{0};
        uint32_t height{0};
        std::array<std::vector<uint8_t>, 6> faces;
    };

    virtual ~RayTracer() = default;

    void setScene(std::shared_ptr<Scene const> scene);

    void setSkybox(std::shared_ptr<Skybox const> skybox);

    void resize(uint32_t width, uint32_t height);

    void reset();

    virtual void render(glm::mat4 const& cameraToWorld, glm::mat4 const& rasterToCamera) = 0;

    virtual gfx::Texture2D const& frame() const = 0;

    uint32_t completedSamples() const noexcept {
        return m_completedSamples;
    }

    uint32_t width() const noexcept {
        return m_width;
    }

    uint32_t height() const noexcept {
        return m_height;
    }

protected:
    std::shared_ptr<Scene const> m_scene;
    std::shared_ptr<Skybox const> m_skybox;

    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_completedSamples{0};

protected:
    virtual void onSceneChanged() {}

    virtual void onSkyboxChanged() {}

    virtual void onResize() = 0;

    virtual void onReset() {}
};
