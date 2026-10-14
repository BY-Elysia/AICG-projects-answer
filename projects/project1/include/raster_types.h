#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include <scene/light/light.h>
#include <graphics/texture/texture2d.h>
#include "cpu_texture2d.h"

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

struct RasterVertex {
    // Original mesh-local position and normal, before node and per-frame transforms.
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    // TEXCOORD_0 used by all supported helmet material maps.
    glm::vec2 uv{0.0f};
};

struct Mesh {
    std::vector<RasterVertex> vertices;
    std::vector<uint32_t> indices;
};

struct DrawItem {
    Mesh const* mesh = nullptr;
    PbrMaterial const* material = nullptr;
    // World transform accumulated through the scene's node hierarchy.
    glm::mat4 transform{1.0f};
};

// Vertex-stage output, captured as four interleaved vec4 values by transform feedback.
struct alignas(16) TransformedVertex {
    glm::vec4 clip{0.0f};
    glm::vec4 worldPosition{0.0f};
    glm::vec4 normal{0.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 uv{0.0f};
};

static_assert(sizeof(TransformedVertex) == 64);

/* Packed triangle payload; fourteen vec4 texels preserve the C++/GLSL record layout. */
struct alignas(16) PreparedVertex {
    // xy is pixel-space position, z is normalized depth, and w is 1 / clip-space w.
    // The reciprocal is required to perform perspective-correct interpolation in both backends.
    glm::vec4 screenAndInvW{0.0f};
    // xyz is the world-space position to interpolate for lighting.
    glm::vec4 worldPosition{0.0f};
    // xyz is the unnormalized world-space normal.
    glm::vec4 normal{0.0f, 1.0f, 0.0f, 0.0f};
    // xy stores UV; vec4 padding preserves packed texel alignment.
    glm::vec4 uv{0.0f};
};

struct alignas(16) PreparedTriangle {
    // Clockwise/back-facing triangles are removed before this stage.
    PreparedVertex vertices[3];
    // UV-derived world-space T basis for normal mapping.
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    // UV-derived world-space B basis for normal mapping.
    glm::vec4 bitangent{0.0f, 1.0f, 0.0f, 1.0f};
};

static_assert(sizeof(PreparedVertex) == 64);
static_assert(sizeof(PreparedTriangle) == 224);

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
    // CPU wall time or GPU GL_TIME_ELAPSED result.
    float milliseconds = 0.0f;
    // Number after clipping; one source triangle can become several.
    uint32_t triangleCount = 0;
    // GPU mode builds ordered, dynamically sized tile lists on CPU before drawing.
    // Reported separately from the GPU visibility/shading timer.
    float binningMilliseconds = 0.0f;
    // Geometry preparation wall time; GPU mode includes vertex draw and result readback.
    float preparationMilliseconds = 0.0f;
    // GPU vertex shader and transform-feedback capture time; zero for the CPU backend.
    float vertexMilliseconds = 0.0f;
};
