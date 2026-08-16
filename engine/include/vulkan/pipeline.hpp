#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace OLIA_ENGINE
{
    struct GraphicsPipeline
    {
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    };

    GraphicsPipeline CreateGraphicsPipeline(
        const std::string &vertFile,
        const std::string &fragFile,
        VkExtent2D swapchainExtent,
        VkDevice device,
        const VkAllocationCallbacks *allocator,
        VkRenderPass renderPass,
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
        VkPipelineCache pipelineCache = VK_NULL_HANDLE,
        uint32_t contantSize = sizeof(uint32_t));

    GraphicsPipeline CreateParticlePipeline(
        const std::string &vertFile,
        const std::string &fragFile,
        VkExtent2D swapchainExtent,
        VkDevice device,
        const VkAllocationCallbacks *allocator,
        VkRenderPass renderPass,
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
        VkPipelineCache pipelineCache = VK_NULL_HANDLE);

    VkShaderModule CreateShaderModule(
        const std::vector<char> &code,
        VkDevice device,
        const VkAllocationCallbacks *allocator);

    void DestroyGraphicsPipeline(
        GraphicsPipeline &graphicsPipeline,
        VkDevice device,
        const VkAllocationCallbacks *allocator);
}