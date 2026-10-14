#version 410 core

layout(location = 0) in vec4 aClip;
layout(location = 1) in vec4 aWorldPosition;
layout(location = 2) in vec4 aNormal;
layout(location = 3) in vec4 aUv;

out VertexData {
    vec4 clip;
    vec4 worldPosition;
    vec4 normal;
    vec4 uv;
} vertexData;

void main() {
    vertexData.clip = aClip;
    vertexData.worldPosition = aWorldPosition;
    vertexData.normal = aNormal;
    vertexData.uv = aUv;
    gl_Position = aClip;
}
