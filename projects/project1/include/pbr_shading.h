#pragma once

#include <algorithm>
#include <cmath>

#include "raster_types.h"

namespace pbr {
inline glm::vec3 srgbToLinear(const glm::vec3& value) {
    glm::vec3 result;
    for (int i = 0; i < 3; ++i) {
        result[i] =
            value[i] <= 0.04045f ? value[i] / 12.92f : std::pow((value[i] + 0.055f) / 1.055f, 2.4f);
    }
    return result;
}

// Evaluates the separable Smith joint masking-shadowing term used by pbr_viewer/pbr.frag.
inline float getGeometryTerm(float nDotL, float nDotV, float alphaRoughness) {
    const float alphaSquared = alphaRoughness * alphaRoughness;
    const float attenuationL =
        2.0f * nDotL
        / (nDotL + std::sqrt(alphaSquared + (1.0f - alphaSquared) * nDotL * nDotL));
    const float attenuationV =
        2.0f * nDotV
        / (nDotV + std::sqrt(alphaSquared + (1.0f - alphaSquared) * nDotV * nDotV));
    return attenuationL * attenuationV;
}

inline glm::vec3 shade(
    const PbrMaterial& material, const FrameState& frame, const glm::vec3& worldPosition,
    glm::vec3 normal, const glm::vec3& tangent, const glm::vec3& bitangent, const glm::vec2& uv) {
    constexpr float kPi = 3.14159265358979323846f;
    // Read base color in linear space; metallic-roughness and normal maps remain linear data.
    // DamagedHelmet declares the default glTF LINEAR minification filter instead of a mipmap
    // filter, so every material map below samples only level zero, matching the GPU path.
    glm::vec4 baseColor = material.baseColorFactor;
    if (material.hasTexture[PbrMaterial::BaseColor]) {
        const glm::vec4 sampledColor =
            material.textures[PbrMaterial::BaseColor].cpuTexture->sample(uv);
        baseColor *= glm::vec4(srgbToLinear(glm::vec3(sampledColor)), sampledColor.a);
    }
    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    if (material.hasTexture[PbrMaterial::MetallicRoughness]) {
        const glm::vec4 sampledColor =
            material.textures[PbrMaterial::MetallicRoughness].cpuTexture->sample(uv);
        roughness *= sampledColor.g;
        metallic *= sampledColor.b;
    }
    roughness = std::clamp(roughness, 0.045f, 1.0f);
    metallic = std::clamp(metallic, 0.0f, 1.0f);

    // Convert a tangent-space normal-map sample into the interpolated world-space frame.
    normal = glm::normalize(normal);
    if (material.hasTexture[PbrMaterial::Normal]) {
        const glm::vec3 tangentNormal =
            glm::vec3(material.textures[PbrMaterial::Normal].cpuTexture->sample(uv)) * 2.0f
            - 1.0f;
        const glm::vec3 orthogonalTangent =
            glm::normalize(tangent - normal * glm::dot(normal, tangent));
        normal = glm::normalize(
            glm::mat3(orthogonalTangent, glm::normalize(bitangent), normal) * tangentNormal);
    }

    // Evaluate the metallic-roughness microfacet BRDF for the single directional light.
    const glm::vec3 viewDirection = glm::normalize(frame.cameraPosition - worldPosition);
    const glm::vec3 lightDirection = glm::normalize(-frame.directionalLight.transform.getFront());
    const glm::vec3 halfway = glm::normalize(viewDirection + lightDirection);
    const float nDotL = std::max(glm::dot(normal, lightDirection), 0.0f);
    const float nDotV = std::max(glm::dot(normal, viewDirection), 0.0f);
    const float nDotH = std::max(glm::dot(normal, halfway), 0.0f);
    const float vDotH = std::max(glm::dot(viewDirection, halfway), 0.0f);
    const float alphaRoughness = roughness * roughness;
    const float alphaSquared = alphaRoughness * alphaRoughness;
    const float denominator = nDotH * nDotH * (alphaSquared - 1.0f) + 1.0f;
    const float distribution = alphaSquared / std::max(kPi * denominator * denominator, 1e-5f);
    const float geometry = getGeometryTerm(nDotL, nDotV, alphaRoughness);
    const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), glm::vec3(baseColor), metallic);
    const glm::vec3 fresnel = f0 + (glm::vec3(1.0f) - f0) * std::pow(1.0f - vDotH, 5.0f);
    const glm::vec3 specular =
        fresnel * distribution * geometry / std::max(4.0f * nDotL * nDotV, 1e-4f);
    const glm::vec3 diffuse =
        (glm::vec3(1.0f) - fresnel) * (1.0f - metallic) * glm::vec3(baseColor) / kPi;
    glm::vec3 color = (diffuse + specular) * frame.directionalLight.color
                      * frame.directionalLight.intensity * nDotL;
    // Ambient occlusion modulates the direct result, then emissive is added without lighting.
    if (material.hasTexture[PbrMaterial::Occlusion]) {
        const float occlusion =
            material.textures[PbrMaterial::Occlusion].cpuTexture->sample(uv).r;
        color *= 1.0f + material.occlusionStrength * (occlusion - 1.0f);
    }
    glm::vec3 emissive = material.emissiveFactor;
    if (material.hasTexture[PbrMaterial::Emissive]) {
        emissive *= srgbToLinear(
            glm::vec3(material.textures[PbrMaterial::Emissive].cpuTexture->sample(uv)));
    }

    return color + emissive;
}
}
