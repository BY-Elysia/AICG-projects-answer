#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>
#include <graphics/buffer.h>
#include <graphics/vertex_array.h>

struct Vertex {
    // Original mesh-local position and normal, before node and per-frame transforms.
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    // TEXCOORD_0 used by the supported material maps.
    glm::vec2 uv{0.0f};
};

struct MeshGpuCache {
    gfx::Buffer vertexBuffer;
    gfx::Buffer indexBuffer;
    gfx::VertexArray vertexArray;
    size_t vertexUploadCount = 1;
    size_t indexUploadCount = 1;

    MeshGpuCache(std::vector<Vertex> const& vertices, std::vector<uint32_t> const& indices)
        : vertexBuffer(gfx::Buffer::UsageFlags::VertexBuffer,
                       vertices.size() * sizeof(Vertex),
                       gfx::Buffer::Usage::DynamicDraw,
                       vertices.data())
        , indexBuffer(gfx::Buffer::UsageFlags::IndexBuffer,
                      indices.size() * sizeof(uint32_t),
                      gfx::Buffer::Usage::DynamicDraw,
                      indices.data()) {
        vertexArray.setFloatAttribute(
            vertexBuffer, {0, 3, gfx::VertexArray::DataType::Float32, sizeof(Vertex), offsetof(Vertex, position)});
        vertexArray.setFloatAttribute(
            vertexBuffer, {1, 3, gfx::VertexArray::DataType::Float32, sizeof(Vertex), offsetof(Vertex, normal)});
        vertexArray.setFloatAttribute(
            vertexBuffer, {2, 2, gfx::VertexArray::DataType::Float32, sizeof(Vertex), offsetof(Vertex, uv)});
        vertexArray.setIndexBuffer(indexBuffer);
    }

    void refreshVertices(std::vector<Vertex> const& vertices) {
        size_t const bytes = vertices.size() * sizeof(Vertex);
        if (vertexBuffer.getSize() != bytes) {
            vertexBuffer.resize(bytes, gfx::Buffer::Usage::DynamicDraw);
        }
        vertexBuffer.write(vertices.data());
        ++vertexUploadCount;
    }

    void refreshIndices(std::vector<uint32_t> const& indices) {
        size_t const bytes = indices.size() * sizeof(uint32_t);
        if (indexBuffer.getSize() != bytes) {
            indexBuffer.resize(bytes, gfx::Buffer::Usage::DynamicDraw);
        }
        indexBuffer.write(indices.data());
        ++indexUploadCount;
    }
};

struct Mesh {
    Mesh() = default;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;
    Mesh(Mesh const&) = delete;
    Mesh& operator=(Mesh const&) = delete;

    std::vector<Vertex> const& vertices() const {
        return m_vertices;
    }

    // Every mutable access invalidates the cached GPU input.
    std::vector<Vertex>& editVertices() {
        m_gpuVerticesDirty = true;
        return m_vertices;
    }

    std::vector<uint32_t> const& indices() const {
        return m_indices;
    }

    std::vector<uint32_t>& editIndices() {
        m_gpuIndicesDirty = true;
        return m_indices;
    }

    // Lazily upload the indexed input; later draws reuse it until the data changes.
    gfx::VertexArray const& gpuVertexArray() const {
        if (m_vertices.empty() || m_indices.empty()) {
            throw std::invalid_argument("Cannot upload a mesh without vertices and indices");
        }
        if (!m_gpuCache) {
            m_gpuCache = std::make_unique<MeshGpuCache>(m_vertices, m_indices);
            m_gpuVerticesDirty = false;
            m_gpuIndicesDirty = false;
        }
        else {
            if (m_gpuVerticesDirty) {
                m_gpuCache->refreshVertices(m_vertices);
                m_gpuVerticesDirty = false;
            }
            if (m_gpuIndicesDirty) {
                m_gpuCache->refreshIndices(m_indices);
                m_gpuIndicesDirty = false;
            }
        }
        return m_gpuCache->vertexArray;
    }

    size_t gpuUploadCount() const {
        return m_gpuCache ? m_gpuCache->vertexUploadCount : 0;
    }

    size_t gpuIndexUploadCount() const {
        return m_gpuCache ? m_gpuCache->indexUploadCount : 0;
    }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    mutable std::unique_ptr<MeshGpuCache> m_gpuCache;
    mutable bool m_gpuVerticesDirty = true;
    mutable bool m_gpuIndicesDirty = true;
};
