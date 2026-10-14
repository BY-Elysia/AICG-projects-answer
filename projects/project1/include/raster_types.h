#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include <scene/light/light.h>
#include <graphics/texture/texture2d.h>

#include "cpu_texture2d.h"
#include "mesh.h"

struct UnifiedTexture2D {
    // CPU texture abstraction owned by the material for CPU rasterizer sampling.
    std::unique_ptr<CpuTexture2D> cpuTexture;
    // GPU texture object shared with the fragment shading pass through the gfx::Texture2D API.
    std::unique_ptr<gfx::Texture2D> gpuTexture;
};

struct PbrMaterial {
    // glTF baseColorFactor, multiplied with the base-color texture.
    glm::vec4 baseColorFactor{1.0f};
    // glTF emissiveFactor, added after direct-light evaluation.
    glm::vec3 emissiveFactor{0.0f};
    // Scalar fallback/multiplier for the metallic texture's B channel.
    float metallicFactor = 1.0f;
    // Scalar fallback/multiplier for the roughness texture's G channel.
    float roughnessFactor = 1.0f;
    // Weight applied to the occlusion texture's R channel.
    float occlusionStrength = 1.0f;

    enum TextureSlot : int { BaseColor = 0, MetallicRoughness, Normal, Occlusion, Emissive };

    // Avoid sampling an unbound texture when a glTF slot is absent.
    std::array<bool, 5> hasTexture{};
    // Identical texture set exposed to both rasterizers.
    std::array<UnifiedTexture2D, 5> textures;
};

struct DrawItem {
    Mesh const* mesh = nullptr;
    PbrMaterial const* material = nullptr;
    // World transform accumulated through the scene's node hierarchy.
    glm::mat4 transform{1.0f};
};

// Shared vertex-stage layout: CPU prepares these values, GPU captures them as four interleaved vec4s.
struct alignas(16) TransformedVertex {
    glm::vec4 clip{0.0f};
    glm::vec4 worldPosition{0.0f};
    glm::vec4 normal{0.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 uv{0.0f};
};

static_assert(sizeof(TransformedVertex) == 64);

struct FrameState {
    // Shared per-frame input. Both rasterizers use the same camera and light for comparison.
    // Optional scene-wide transform applied after every node's world transform.
    glm::mat4 model{1.0f};
    // Camera world-to-view transform.
    glm::mat4 view{1.0f};
    // Perspective projection used to create screen-space triangles.
    glm::mat4 projection{1.0f};
    // Needed to construct the PBR view vector.
    glm::vec3 cameraPosition{0.0f, 0.0f, 5.0f};
    // Keeps transform, color, and intensity as one component.
    DirectionalLight directionalLight;
    // Raster target size in physical framebuffer pixels.
    glm::ivec2 extent{1280, 720};
};

struct RendererStats {
    // CPU raster wall time or most recently available asynchronous GPU query result.
    float milliseconds = 0.0f;
    // CPU: triangles after clipping; GPU: submitted source triangles.
    uint32_t triangleCount = 0;
    // CPU geometry preparation, or CPU submission wall time for the GPU renderer.
    float preparationMilliseconds = 0.0f;
};
