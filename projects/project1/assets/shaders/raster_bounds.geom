#version 410 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

in VertexData {
    vec4 clip;
    vec4 worldPosition;
    vec4 normal;
    vec4 uv;
} vertexData[];

flat out vec3 gWorld[3];
flat out vec3 gNormal[3];
flat out vec2 gUv[3];

uniform vec2 uExtent;

void emitCorner(vec2 corner) {
    for (int i = 0; i < 3; ++i) {
        gWorld[i] = vertexData[i].worldPosition.xyz;
        gNormal[i] = vertexData[i].normal.xyz;
        gUv[i] = vertexData[i].uv.xy;
    }
    gl_Position = vec4(corner, 0.0, 1.0);
    EmitVertex();
}

void main() {
    vec2 minimum = vec2(-1.0);
    vec2 maximum = vec2(1.0);
    if (all(greaterThan(vec3(vertexData[0].clip.w, vertexData[1].clip.w, vertexData[2].clip.w),
                        vec3(0.00001)))) {
        minimum = vec2(1e20);
        maximum = vec2(-1e20);
        for (int i = 0; i < 3; ++i) {
            vec2 ndc = vertexData[i].clip.xy / vertexData[i].clip.w;
            minimum = min(minimum, ndc);
            maximum = max(maximum, ndc);
        }
        // Cover boundary pixels despite floating-point rounding in the bounding pass.
        vec2 padding = vec2(2.0) / uExtent;
        minimum = clamp(minimum - padding, vec2(-1.0), vec2(1.0));
        maximum = clamp(maximum + padding, vec2(-1.0), vec2(1.0));
    }
    if (any(greaterThanEqual(minimum, maximum))) {
        return;
    }

    emitCorner(minimum);
    emitCorner(vec2(maximum.x, minimum.y));
    emitCorner(vec2(minimum.x, maximum.y));
    emitCorner(maximum);
    EndPrimitive();
}
