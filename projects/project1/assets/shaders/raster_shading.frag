#version 410 core

flat in vec3 gWorld[3];
flat in vec3 gNormal[3];
flat in vec2 gUv[3];

layout(location = 0) out vec4 outColor;

uniform mat4 uViewProjection;
uniform mat4 uInverseViewProjection;
uniform vec2 uExtent;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform vec4 uBaseColorFactor;
uniform vec3 uEmissiveFactor;
uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform float uOcclusionStrength;
uniform bool uHasTexture[5];
uniform sampler2D uTextures[5];

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
    vec2 ndc = gl_FragCoord.xy / uExtent * 2.0 - 1.0;
    vec4 nearPoint = uInverseViewProjection * vec4(ndc, -1.0, 1.0);
    vec4 farPoint = uInverseViewProjection * vec4(ndc, 1.0, 1.0);
    vec3 origin = nearPoint.xyz / nearPoint.w;
    vec3 direction = farPoint.xyz / farPoint.w - origin;

    vec3 edge1 = gWorld[1] - gWorld[0];
    vec3 edge2 = gWorld[2] - gWorld[0];
    vec3 p = cross(direction, edge2);
    float determinant = dot(edge1, p);
    // Back faces and degenerate triangles have no visible samples.
    if (determinant <= 0.0) {
        discard;
    }
    vec3 relative = origin - gWorld[0];
    float b = dot(relative, p) / determinant;
    vec3 q = cross(relative, edge1);
    float c = dot(direction, q) / determinant;
    float distanceAlongRay = dot(edge2, q) / determinant;
    if (b < 0.0 || c < 0.0 || b + c > 1.0 || distanceAlongRay < 0.0 || distanceAlongRay > 1.0) {
        discard;
    }
    vec3 weights = vec3(1.0 - b - c, b, c);
    vec3 worldPosition = weights.x * gWorld[0] + weights.y * gWorld[1] + weights.z * gWorld[2];
    vec3 normal = weights.x * gNormal[0] + weights.y * gNormal[1] + weights.z * gNormal[2];
    vec2 uv = weights.x * gUv[0] + weights.y * gUv[1] + weights.z * gUv[2];
    vec2 deltaUv1 = gUv[1] - gUv[0];
    vec2 deltaUv2 = gUv[2] - gUv[0];
    float uvDeterminant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
    vec3 tangent = vec3(1.0, 0.0, 0.0);
    vec3 bitangent = vec3(0.0, 1.0, 0.0);
    if (abs(uvDeterminant) > 1e-8) {
        tangent = normalize((edge1 * deltaUv2.y - edge2 * deltaUv1.y) / uvDeterminant);
        bitangent = normalize((-edge1 * deltaUv2.x + edge2 * deltaUv1.x) / uvDeterminant);
    }
    vec4 clip = uViewProjection * vec4(worldPosition, 1.0);
    float depth = clip.z / clip.w * 0.5 + 0.5;
    if (depth < 0.0 || depth >= 1.0) {
        discard;
    }
    gl_FragDepth = depth;
    outColor = vec4(shade(worldPosition, normal, tangent, bitangent, uv), 1.0);
}
