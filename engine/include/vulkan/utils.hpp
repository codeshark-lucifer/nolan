#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace OLIA_ENGINE
{
    extern const std::vector<const char *> validationLayers;

    bool checkValidationLayerSupport();
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    // Read binary file (e.g. SPIR-V shader files)
    std::vector<char> ReadFile(const std::string &filename);

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer);

    void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
}