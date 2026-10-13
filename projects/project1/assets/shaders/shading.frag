#version 410 core

struct PreparedVertex {
    // xy stores raster position, z stores depth, and w stores reciprocal clip-space w.
    vec4 screenAndInvW;
    // xyz stores the world-space position used to build lighting vectors.
    vec4 worldPosition;
    // xyz stores the world-space vertex normal used as the geometric normal source.
    vec4 normal;
    // xy stores material UV coordinates.
    vec4 uv;
};

struct PreparedTriangle {
    // The three vertices whose attributes are reconstructed after visibility selection.
    PreparedVertex vertices[3];
    // World-space tangent generated from triangle edges and UV deltas.
    vec4 tangent;
    // World-space bitangent generated from triangle edges and UV deltas.
    vec4 bitangent;
};

// Packed records produced by the shared CPU frontend, and fragment-pass visibility output.
uniform sampler2D uTriangles;
uniform usampler2D uOwner;
layout(location = 0) out vec4 outColor;

vec4 triangleTexel(uint index) {
    uint width = uint(textureSize(uTriangles, 0).x);
    return texelFetch(uTriangles, ivec2(index % width, index / width), 0);
}

PreparedTriangle readTriangle(uint index) {
    PreparedTriangle triangle;
    uint base = index * 14u;
    for (uint corner = 0u; corner < 3u; ++corner) {
        triangle.vertices[corner].screenAndInvW = triangleTexel(base + corner * 4u);
        triangle.vertices[corner].worldPosition = triangleTexel(base + corner * 4u + 1u);
        triangle.vertices[corner].normal = triangleTexel(base + corner * 4u + 2u);
        triangle.vertices[corner].uv = triangleTexel(base + corner * 4u + 3u);
    }
    triangle.tangent = triangleTexel(base + 12u);
    triangle.bitangent = triangleTexel(base + 13u);
    return triangle;
}

// Valid element count in triangles[]; it validates the owner fetched from uOwner.
uniform int uTriangleCount;
// World-space camera position used to construct the V vector.
uniform vec3 uCameraPosition;
// Direction in which directional-light rays travel. Negating it produces surface-to-light L.
uniform vec3 uLightDirection;
// Linear RGB directional-light color.
uniform vec3 uLightColor;
// Scalar directional-light radiance multiplier.
uniform float uLightIntensity;
// glTF baseColorFactor multiplied with the base-color texture.
uniform vec4 uBaseColorFactor;
// glTF emissiveFactor multiplied with the emissive texture.
uniform vec3 uEmissiveFactor;
// glTF metallicFactor multiplied with the metallic-roughness texture's B channel.
uniform float uMetallicFactor;
// glTF roughnessFactor multiplied with the metallic-roughness texture's G channel.
uniform float uRoughnessFactor;
// glTF occlusion strength applied to the occlusion texture's R channel.
uniform float uOcclusionStrength;
// Texture-presence flags avoid sampling an undefined slot.
uniform bool uHasTexture[5];
// Slots are base color, metallic-roughness, normal, occlusion, and emissive respectively.
uniform sampler2D uTextures[5];

const uint noOwner = 0xffffffffu;

float cross2D(vec2 lhs, vec2 rhs) {
    // This exactly matches the signed-area convention used by visibility.frag and the CPU path.
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

vec3 srgbToLinear(vec3 value) {
    vec3 result;
    for (int i = 0; i < 3; ++i) {
        result[i] =
            value[i] <= 0.04045
                ? value[i] / 12.92
                : pow((value[i] + 0.055) / 1.055, 2.4);
    }
    return result;
}

// Evaluates the same separable Smith joint masking-shadowing term as pbr_shading.h.
float getGeometryTerm(float nDotL, float nDotV, float alphaRoughness) {
    float alphaSquared = alphaRoughness * alphaRoughness;
    float attenuationL =
        2.0 * nDotL
        / (nDotL + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotL * nDotL));
    float attenuationV =
        2.0 * nDotV
        / (nDotV + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotV * nDotV));
    return attenuationL * attenuationV;
}

// Matches pbr::shade() in pbr_shading.h. Explicit level-zero sampling avoids fragment
// derivatives changing filtering relative to the CPU backend.
vec3 shade(
    vec3 worldPosition, vec3 normal, vec3 tangent, vec3 bitangent, vec2 uv) {
    const float kPi = 3.14159265358979323846;
    // Read base color in linear space; metallic-roughness and normal maps remain linear data.
    vec4 baseColor = uBaseColorFactor;
    if (uHasTexture[0]) {
        vec4 sampledColor = textureLod(uTextures[0], uv, 0.0);
        baseColor *= vec4(srgbToLinear(sampledColor.rgb), sampledColor.a);
    }
    float roughness = uRoughnessFactor;
    float metallic = uMetallicFactor;
    if (uHasTexture[1]) {
        vec4 sampledColor = textureLod(uTextures[1], uv, 0.0);
        roughness *= sampledColor.g;
        metallic *= sampledColor.b;
    }
    roughness = clamp(roughness, 0.045, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    // Convert a tangent-space normal-map sample into the interpolated world-space frame.
    normal = normalize(normal);
    if (uHasTexture[2]) {
        vec3 tangentNormal = textureLod(uTextures[2], uv, 0.0).xyz * 2.0 - 1.0;
        vec3 orthogonalTangent = normalize(tangent - normal * dot(normal, tangent));
        normal = normalize(mat3(orthogonalTangent, normalize(bitangent), normal) * tangentNormal);
    }

    // Evaluate the metallic-roughness microfacet BRDF for the single directional light.
    vec3 viewDirection = normalize(uCameraPosition - worldPosition);
    vec3 lightDirection = normalize(-uLightDirection);
    vec3 halfway = normalize(viewDirection + lightDirection);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float nDotV = max(dot(normal, viewDirection), 0.0);
    float nDotH = max(dot(normal, halfway), 0.0);
    float vDotH = max(dot(viewDirection, halfway), 0.0);
    float alphaRoughness = roughness * roughness;
    float alphaSquared = alphaRoughness * alphaRoughness;
    float denominator = nDotH * nDotH * (alphaSquared - 1.0) + 1.0;
    float distribution = alphaSquared / max(kPi * denominator * denominator, 1e-5);
    float geometry = getGeometryTerm(nDotL, nDotV, alphaRoughness);
    vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
    vec3 fresnel = f0 + (vec3(1.0) - f0) * pow(1.0 - vDotH, 5.0);
    vec3 specular = fresnel * distribution * geometry / max(4.0 * nDotL * nDotV, 1e-4);
    vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * baseColor.rgb / kPi;
    vec3 color = (diffuse + specular) * uLightColor * uLightIntensity * nDotL;
    // Ambient occlusion modulates the direct result, then emissive is added without lighting.
    if (uHasTexture[3]) {
        float occlusion = textureLod(uTextures[3], uv, 0.0).r;
        color *= 1.0 + uOcclusionStrength * (occlusion - 1.0);
    }
    vec3 emissive = uEmissiveFactor;
    if (uHasTexture[4]) {
        emissive *= srgbToLinear(textureLod(uTextures[4], uv, 0.0).rgb);
    }

    return color + emissive;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    // The owner attachment identifies the nearest primitive selected by the depth comparison.
    uint triangleIndex = texelFetch(uOwner, pixel, 0).r;
    if (triangleIndex == noOwner || triangleIndex >= uint(uTriangleCount)) {
        outColor = vec4(0.025, 0.03, 0.04, 1.0);
        return;
    }

    // Rebuild the winning triangle's affine barycentric coordinates at the pixel center.
    PreparedTriangle triangle = readTriangle(triangleIndex);
    vec2 samplePosition = vec2(pixel) + vec2(0.5);
    vec2 a = triangle.vertices[0].screenAndInvW.xy;
    vec2 b = triangle.vertices[1].screenAndInvW.xy;
    vec2 c = triangle.vertices[2].screenAndInvW.xy;
    vec3 barycentric = vec3(
        cross2D(c - b, samplePosition - b), cross2D(a - c, samplePosition - c),
        cross2D(b - a, samplePosition - a));
    float triangleArea = cross2D(b - a, c - a);
    barycentric /= triangleArea;
    // Multiply by reciprocal w then normalize, giving perspective-correct interpolation weights.
    vec3 corrected = barycentric
                           * vec3(
                               triangle.vertices[0].screenAndInvW.w,
                               triangle.vertices[1].screenAndInvW.w,
                               triangle.vertices[2].screenAndInvW.w);
    vec3 weights = corrected / (corrected.x + corrected.y + corrected.z);
    vec3 worldPosition = vec3(0.0);
    vec3 normal = vec3(0.0);
    vec2 uv = vec2(0.0);
    // Interpolate the complete material input set using those perspective-correct weights.
    for (int corner = 0; corner < 3; ++corner) {
        worldPosition += weights[corner] * triangle.vertices[corner].worldPosition.xyz;
        normal += weights[corner] * triangle.vertices[corner].normal.xyz;
        uv += weights[corner] * triangle.vertices[corner].uv.xy;
    }
    // Shade the visible sample with material textures and direct-light PBR.
    vec3 color = shade(
        worldPosition, normal, triangle.tangent.xyz, triangle.bitangent.xyz, uv);
    outColor = vec4(color, 1.0);
}
