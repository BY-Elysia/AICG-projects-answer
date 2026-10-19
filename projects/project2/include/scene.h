#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "bvh.h"
#include "material.h"
#include "model.h"

class Scene {
public:
    Scene(std::vector<Sphere> const& spheres,
          std::vector<Model const*> const& models,
          std::vector<glm::mat4> const& transforms,
          std::vector<Material> const& sphereMaterials,
          std::vector<Material> const& modelMaterials);

    std::vector<Sphere> spheres;
    std::vector<Primitive> primitives;
    std::vector<Triangle> triangles;
    std::vector<Vertex> vertices;
    std::vector<Material> materials;
    std::vector<BVH::Node> nodes;
};
