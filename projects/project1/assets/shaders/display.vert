#version 410 core

out vec2 fTexCoord;

void main() {
    // IDs 0, 1, 2 generate (-1,-1), (3,-1), (-1,3), covering the viewport with one triangle.
    vec2 uv = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    fTexCoord = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
