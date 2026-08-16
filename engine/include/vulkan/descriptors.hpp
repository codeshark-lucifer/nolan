#pragma once
#include "vulkan/vk_context.h"

namespace OLIA_ENGINE
{
    void CreateDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout *descriptorSetLayout, const VkAllocationCallbacks *allocator);
    void CreateDescriptorPool(VkDevice device, VkDescriptorPool *descriptorPool, const VkAllocationCallbacks *allocator);
    void CreateDescriptorSets(
        VkDevice device, VkDescriptorSetLayout &descriptorSetLayout,
        VkDescriptorPool descriptorPool, std::vector<VkDescriptorSet> &descriptorSets,
        UniformBuffers &uniforms, const VkAllocationCallbacks *allocator);
}