#version 410 core

in vec2 fTexCoord;
out vec4 outColor;
uniform sampler2D uHdrColor;

void main() {
    // Convert the shared HDR output to displayable LDR sRGB after either rasterizer finishes.
    vec3 color = texture(uHdrColor, fTexCoord).rgb;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
