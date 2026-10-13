#version 410 core

uniform sampler2D uTriangles;
uniform usampler2D uTileRanges;
uniform usampler2D uTileIndices;
uniform int uTileSize;
layout(location = 0) out uint outOwner;

vec4 triangleTexel(uint index) {
    uint width = uint(textureSize(uTriangles, 0).x);
    return texelFetch(uTriangles, ivec2(index % width, index / width), 0);
}

uint tileIndex(uint entry) {
    uint width = uint(textureSize(uTileIndices, 0).x);
    return texelFetch(uTileIndices, ivec2(entry % width, entry / width), 0).r;
}

float cross2D(vec2 a, vec2 b) {
    return a.x * b.y - a.y * b.x;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    uvec2 range = texelFetch(uTileRanges, pixel / uTileSize, 0).rg;
    vec2 samplePosition = vec2(pixel) + vec2(0.5);
    float closestDepth = 1.0;
    uint owner = 0xffffffffu;
    // The range has no fixed per-tile capacity. CPU binning retains submission order.
    for (uint entry = 0u; entry < range.y; ++entry) {
        uint triangle = tileIndex(range.x + entry);
        vec4 a = triangleTexel(triangle * 14u);
        vec4 b = triangleTexel(triangle * 14u + 4u);
        vec4 c = triangleTexel(triangle * 14u + 8u);
        float area = cross2D(b.xy - a.xy, c.xy - a.xy);
        if (area <= 0.0) {
            continue;
        }
        vec3 barycentric = vec3(
            cross2D(c.xy - b.xy, samplePosition - b.xy),
            cross2D(a.xy - c.xy, samplePosition - c.xy),
            cross2D(b.xy - a.xy, samplePosition - a.xy));
        if (any(lessThan(barycentric, vec3(0.0)))) {
            continue;
        }
        barycentric /= area;
        float depth = dot(barycentric, vec3(a.z, b.z, c.z));
        // Strict comparison chooses the first submitted triangle on equal depth, like CPU.
        if (depth < closestDepth) {
            closestDepth = depth;
            owner = triangle;
        }
    }
    // Every pixel is overwritten, including background, so no texture clear is required.
    outOwner = owner;
}
