#version 450

layout(location = 0) in vec4 inPos;   // xyz: position, w: current life
layout(location = 1) in vec4 inVel;   // xyz: velocity, w: maxLife
layout(location = 2) in vec4 inColor; // rgba

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

void main() {
    vec4 viewPos = camera.view * vec4(inPos.xyz, 1.0);
    gl_Position = camera.proj * viewPos;

    // Perspective point size attenuation
    float lifeRatio = clamp(inPos.w / inVel.w, 0.0, 1.0);
    float dist = length(viewPos.xyz);
    gl_PointSize = clamp((18.0 / dist) * (lifeRatio * 0.8 + 0.2), 2.0, 48.0);

    outColor = inColor;
}