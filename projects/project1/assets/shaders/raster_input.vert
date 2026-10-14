#version 410 core

layout(location = 0) in vec4 aClip;
layout(location = 1) in vec3 aWorldPosition;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec2 aUv;

out VertexData {
    vec4 clip;
    vec3 worldPosition;
    vec3 normal;
    vec2 uv;
} vertexData;

void main() {
    vertexData.clip = aClip;
    vertexData.worldPosition = aWorldPosition;
    vertexData.normal = aNormal;
    vertexData.uv = aUv;
    gl_Position = aClip;
}
