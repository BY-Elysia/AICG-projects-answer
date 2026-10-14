#pragma once

#include <cstdint>
#include <vector>

#include "aabb.h"
#include "primitive.h"
#include "sphere.h"
#include "triangle.h"
#include "vertex.h"

class BVH {
public:
    enum class NodeType : int { Interior = 0, Leaf = 1 };

    struct Node {
        AABB bounds;
        NodeType type{NodeType::Leaf};
        int first{0};
        int second{0};
    };

    BVH(std::vector<Sphere> const& spheres,
        std::vector<Primitive> const& primitives,
        std::vector<Triangle> const& triangles,
        std::vector<Vertex> const& vertices);

    std::vector<Node> const& nodes() const noexcept {
        return m_nodes;
    }

    std::vector<uint32_t> const& orderedPrimitiveIndices() const noexcept {
        return m_orderedPrimitiveIndices;
    }

    size_t height() const noexcept {
        return m_height;
    }

private:
    std::vector<AABB> m_primitiveBounds;
    std::vector<Node> m_nodes;
    std::vector<uint32_t> m_orderedPrimitiveIndices;
    size_t m_height{0};

    int build(size_t begin, size_t end, size_t depth);
};
