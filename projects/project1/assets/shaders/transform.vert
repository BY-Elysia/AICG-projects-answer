#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uMeshTransform;
uniform mat4 uModel;
uniform mat4 uModelViewProjection;
uniform mat3 uNodeNormalTransform;
uniform mat3 uNormalTransform;

// Captured before fixed-function clipping, in the TransformedVertex record order.
out precise vec4 tfClip;
out precise vec3 tfWorldPosition;
out precise vec3 tfNormal;
out vec2 tfUv;

void main() {
    vec3 position = vec3(uMeshTransform * vec4(aPosition, 1.0));
    vec3 normal = normalize(uNodeNormalTransform * aNormal);
    tfClip = uModelViewProjection * vec4(position, 1.0);
    tfWorldPosition = vec3(uModel * vec4(position, 1.0));
    tfNormal = normalize(uNormalTransform * normal);
    tfUv = aUv;
    gl_Position = tfClip;
}
