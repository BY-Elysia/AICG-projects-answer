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
out precise vec4 tfWorldPosition;
out precise vec4 tfNormal;
out vec4 tfUv;

void main() {
    vec3 position = vec3(uMeshTransform * vec4(aPosition, 1.0));
    vec3 normal = normalize(uNodeNormalTransform * aNormal);
    tfClip = uModelViewProjection * vec4(position, 1.0);
    tfWorldPosition = vec4(vec3(uModel * vec4(position, 1.0)), 1.0);
    tfNormal = vec4(normalize(uNormalTransform * normal), 0.0);
    tfUv = vec4(aUv, 0.0, 0.0);
    gl_Position = tfClip;
}
