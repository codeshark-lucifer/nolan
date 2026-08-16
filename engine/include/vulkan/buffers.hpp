#pragma once

#include <vulkan/vulkan.h>

namespace OLIA_ENGINE
{
    struct Buffer
    {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};

        bool valid() const
        {
            return buffer != VK_NULL_HANDLE &&
                   memory != VK_NULL_HANDLE;
        }
    };


    void CreateBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory,
        const VkAllocationCallbacks* allocator = nullptr
    );


    void CopyBuffer(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        VkDeviceSize size
    );


    void UploadBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkCommandPool commandPool,
        VkQueue queue,
        const void* data,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory,
        const VkAllocationCallbacks* allocator = nullptr
    );


    void CreateUniformBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBuffer& buffer,
        VkDeviceMemory& memory,
        void** mapped,
        const VkAllocationCallbacks* allocator = nullptr
    );


    void DestroyBuffer(
        VkDevice device,
        Buffer& buffer,
        const VkAllocationCallbacks* allocator = nullptr
    );
}