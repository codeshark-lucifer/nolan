#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform MeshPushConstants {
    mat4 model;
    vec4 baseColor;
    vec2 tiling;
    vec2 offset;
    float roughness;
    float metallic;
} push;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = fragTexCoord * push.tiling + push.offset;
    vec4 texColor = texture(texSampler, uv);
    
    outColor = texColor * push.baseColor;
}