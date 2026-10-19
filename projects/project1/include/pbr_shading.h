#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <glm/gtc/color_space.hpp>

#include "raster_types.h"

inline glm::vec3 pbrShade(PbrMaterial const& material,
                          FrameState const& frame,
                          glm::vec3 const& worldPosition,
                          glm::vec3 normal,
                          glm::vec3 const& tangent,
                          glm::vec3 const& bitangent,
                          glm::vec2 const& uv) {
    // Read base color in linear space; metallic-roughness and normal maps remain linear data.
    // DamagedHelmet declares the default glTF LINEAR minification filter instead of a mipmap
    // filter, so every material map below samples only level zero, matching the GPU path.
    glm::vec4 baseColor = material.baseColorFactor;
    if (material.hasTexture[PbrMaterial::BaseColor]) {
        auto const& texture = material.textures[PbrMaterial::BaseColor];
        glm::vec4 const sampledColor = texture.cpuSampler->sample(*texture.cpuTexture, uv);
        baseColor *= glm::vec4(glm::convertSRGBToLinear(glm::vec3(sampledColor)), sampledColor.a);
    }

    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    if (material.hasTexture[PbrMaterial::MetallicRoughness]) {
        auto const& texture = material.textures[PbrMaterial::MetallicRoughness];
        glm::vec4 const sampledColor = texture.cpuSampler->sample(*texture.cpuTexture, uv);
        roughness *= sampledColor.g;
        metallic *= sampledColor.b;
    }

    roughness = std::clamp(roughness, 0.045f, 1.0f);
    metallic = std::clamp(metallic, 0.0f, 1.0f);

    // Convert a tangent-space normal-map sample into the interpolated world-space frame.
    normal = glm::normalize(normal);
    if (material.hasTexture[PbrMaterial::Normal]) {
        auto const& texture = material.textures[PbrMaterial::Normal];
        glm::vec3 const tangentNormal = glm::vec3(texture.cpuSampler->sample(*texture.cpuTexture, uv)) * 2.0f - 1.0f;
        glm::vec3 const orthogonalTangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
        normal = glm::normalize(glm::mat3(orthogonalTangent, glm::normalize(bitangent), normal) * tangentNormal);
    }

    // Evaluate the metallic-roughness microfacet BRDF for the single directional light.
    glm::vec3 const viewDirection = glm::normalize(frame.cameraPosition - worldPosition);
    glm::vec3 const lightDirection = glm::normalize(-frame.directionalLight.transform.getFront());
    glm::vec3 const halfway = glm::normalize(viewDirection + lightDirection);

    float const nDotL = std::max(glm::dot(normal, lightDirection), 0.0f);
    float const nDotV = std::max(glm::dot(normal, viewDirection), 0.0f);
    float const nDotH = std::max(glm::dot(normal, halfway), 0.0f);
    float const vDotH = std::max(glm::dot(viewDirection, halfway), 0.0f);

    float const alphaRoughness = roughness * roughness;
    float const alphaSquared = alphaRoughness * alphaRoughness;
    float const denominator = nDotH * nDotH * (alphaSquared - 1.0f) + 1.0f;
    float const distribution = alphaSquared / std::max(std::numbers::pi_v<float> * denominator * denominator, 1e-5f);

    // Separable Smith joint masking-shadowing term.
    float const attenuationL = 2.0f * nDotL / (nDotL + std::sqrt(alphaSquared + (1.0f - alphaSquared) * nDotL * nDotL));
    float const attenuationV = 2.0f * nDotV / (nDotV + std::sqrt(alphaSquared + (1.0f - alphaSquared) * nDotV * nDotV));
    float const geometry = attenuationL * attenuationV;

    glm::vec3 const f0 = glm::mix(glm::vec3(0.04f), glm::vec3(baseColor), metallic);
    glm::vec3 const fresnel = f0 + (glm::vec3(1.0f) - f0) * std::pow(1.0f - vDotH, 5.0f);

    glm::vec3 const specular = fresnel * distribution * geometry / std::max(4.0f * nDotL * nDotV, 1e-4f);
    glm::vec3 const diffuse =
        (glm::vec3(1.0f) - fresnel) * (1.0f - metallic) * glm::vec3(baseColor) / std::numbers::pi_v<float>;

    glm::vec3 color = (diffuse + specular) * frame.directionalLight.color * frame.directionalLight.intensity * nDotL;

    // Ambient occlusion modulates the direct result, then emissive is added without lighting.
    if (material.hasTexture[PbrMaterial::Occlusion]) {
        auto const& texture = material.textures[PbrMaterial::Occlusion];
        float const occlusion = texture.cpuSampler->sample(*texture.cpuTexture, uv).r;
        color *= 1.0f + material.occlusionStrength * (occlusion - 1.0f);
    }

    glm::vec3 emissive = material.emissiveFactor;
    if (material.hasTexture[PbrMaterial::Emissive]) {
        auto const& texture = material.textures[PbrMaterial::Emissive];
        emissive *= glm::convertSRGBToLinear(glm::vec3(texture.cpuSampler->sample(*texture.cpuTexture, uv)));
    }

    return color + emissive;
}
