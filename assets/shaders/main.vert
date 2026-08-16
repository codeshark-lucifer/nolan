#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(push_constant) uniform MeshPushConstants {
    mat4 model;
    vec4 baseColor;
    vec2 tiling;
    vec2 offset;
    float roughness;
    float metallic;
} push;

void main() {
    gl_Position = camera.proj * camera.view * push.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
}