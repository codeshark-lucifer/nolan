#pragma once
#include "vulkan/vk_context.h"

namespace OLIA_ENGINE
{
    VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);
    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);

    void CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, const VkAllocationCallbacks *allocator = nullptr);
    VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, const VkAllocationCallbacks *allocator);

    void CreateColorResources(VKContext &ctx);
    void DestroyColorResources(VKContext &ctx);

    void CreateDepthResources(VKContext &ctx);
    void DestroyDepthResources(VKContext &ctx);

    void CreateTextureImage(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, UniformBuffers &uniforms, const VkAllocationCallbacks *allocator = (const VkAllocationCallbacks *)nullptr);
    void CreateTextureImageView(VkDevice device, UniformBuffers &uniforms, const VkAllocationCallbacks *allocator = (const VkAllocationCallbacks *)nullptr);
    void CreateTextureSampler(
        VkDevice device, VkPhysicalDevice physicalDevice,
        VkSampler &textureSampler, const VkAllocationCallbacks *allocator = (const VkAllocationCallbacks *)nullptr);
}