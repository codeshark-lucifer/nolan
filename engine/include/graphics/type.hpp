#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <array>

#include <vulkan/vulkan.h>

struct UIPushConstants
{
    glm::mat4 projection{1.0f};               // 64 bytes
    glm::mat4 model{1.0f};                    // 64 bytes
};

struct MeshPushConstants
{
    glm::mat4 model{1.0f};                       // 64 bytes (Vertex stage)
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f}; // 16 bytes (Fragment stage)
    glm::vec2 tiling{1.0f, 1.0f};                // 8 bytes
    glm::vec2 offset{0.0f, 0.0f};                // 8 bytes
    float roughness{0.5f};                       // 4 bytes
    float metallic{0.0f};                        // 4 bytes
    float padding[6]{0.0f};                      // Pad to 128 bytes
};

struct CameraUBO // Camera Uniform Buffer Object (128 bytes)
{
    alignas(16) glm::mat4 view{1.0f}; // Offset 0
    alignas(16) glm::mat4 proj{1.0f}; // Offset 64
};
