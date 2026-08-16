#include "vulkan/buffers.hpp"
#include "vulkan/utils.hpp"

#include <cstring>
#include <stdexcept>

namespace OLIA_ENGINE
{
    void CreateBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer &buffer,
        VkDeviceMemory &bufferMemory,
        const VkAllocationCallbacks *allocator)
    {
        // --------------------------------------------------------
        // Create buffer
        // --------------------------------------------------------

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(
                device,
                &bufferInfo,
                allocator,
                &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Failed to create Vulkan buffer!");
        }

        // --------------------------------------------------------
        // Get memory requirements
        // --------------------------------------------------------

        VkMemoryRequirements memRequirements{};

        vkGetBufferMemoryRequirements(
            device,
            buffer,
            &memRequirements);

        // --------------------------------------------------------
        // Allocate memory
        // --------------------------------------------------------

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        allocInfo.memoryTypeIndex =
            findMemoryType(
                physicalDevice,
                memRequirements.memoryTypeBits,
                properties);

        if (vkAllocateMemory(
                device,
                &allocInfo,
                allocator,
                &bufferMemory) != VK_SUCCESS)
        {
            vkDestroyBuffer(
                device,
                buffer,
                allocator);

            throw std::runtime_error(
                "Failed to allocate Vulkan buffer memory!");
        }

        // --------------------------------------------------------
        // Bind memory
        // --------------------------------------------------------

        VkResult result = vkBindBufferMemory(
            device,
            buffer,
            bufferMemory,
            0);

        if (result != VK_SUCCESS)
        {
            vkFreeMemory(
                device,
                bufferMemory,
                allocator);

            vkDestroyBuffer(
                device,
                buffer,
                allocator);

            throw std::runtime_error(
                "Failed to bind Vulkan buffer memory!");
        }
    }

    void CopyBuffer(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        allocInfo.level =
            VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        allocInfo.commandPool =
            commandPool;

        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;

        if (vkAllocateCommandBuffers(
                device,
                &allocInfo,
                &commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "Failed to allocate copy command buffer!");
        }

        // --------------------------------------------------------
        // Begin command buffer
        // --------------------------------------------------------

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        beginInfo.flags =
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(
                commandBuffer,
                &beginInfo) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(
                device,
                commandPool,
                1,
                &commandBuffer);

            throw std::runtime_error(
                "Failed to begin copy command buffer!");
        }

        // --------------------------------------------------------
        // Copy
        // --------------------------------------------------------

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;

        vkCmdCopyBuffer(
            commandBuffer,
            srcBuffer,
            dstBuffer,
            1,
            &copyRegion);

        // --------------------------------------------------------
        // End command buffer
        // --------------------------------------------------------

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(
                device,
                commandPool,
                1,
                &commandBuffer);

            throw std::runtime_error(
                "Failed to end copy command buffer!");
        }

        // --------------------------------------------------------
        // Submit
        // --------------------------------------------------------

        VkSubmitInfo submitInfo{};
        submitInfo.sType =
            VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (vkQueueSubmit(
                queue,
                1,
                &submitInfo,
                VK_NULL_HANDLE) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(
                device,
                commandPool,
                1,
                &commandBuffer);

            throw std::runtime_error(
                "Failed to submit copy command buffer!");
        }

        // --------------------------------------------------------
        // Wait
        // --------------------------------------------------------

        vkQueueWaitIdle(queue);

        // --------------------------------------------------------
        // Free command buffer
        // --------------------------------------------------------

        vkFreeCommandBuffers(
            device,
            commandPool,
            1,
            &commandBuffer);
    }

    void UploadBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkCommandPool commandPool,
        VkQueue queue,
        const void *data,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkBuffer &buffer,
        VkDeviceMemory &bufferMemory,
        const VkAllocationCallbacks *allocator)
    {
        // --------------------------------------------------------
        // Create staging buffer
        // --------------------------------------------------------

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

        CreateBuffer(
            device,
            physicalDevice,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory,
            allocator);

        // --------------------------------------------------------
        // Copy CPU data -> staging buffer
        // --------------------------------------------------------

        void *mapped = nullptr;

        if (vkMapMemory(
                device,
                stagingMemory,
                0,
                size,
                0,
                &mapped) != VK_SUCCESS)
        {
            vkDestroyBuffer(
                device,
                stagingBuffer,
                allocator);

            vkFreeMemory(
                device,
                stagingMemory,
                allocator);

            throw std::runtime_error(
                "Failed to map staging buffer!");
        }

        std::memcpy(
            mapped,
            data,
            static_cast<size_t>(size));

        vkUnmapMemory(
            device,
            stagingMemory);

        // --------------------------------------------------------
        // Create GPU-local buffer
        // --------------------------------------------------------

        CreateBuffer(
            device,
            physicalDevice,
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            buffer,
            bufferMemory,
            allocator);

        // --------------------------------------------------------
        // Staging -> GPU buffer
        // --------------------------------------------------------

        CopyBuffer(
            device,
            commandPool,
            queue,
            stagingBuffer,
            buffer,
            size);

        // --------------------------------------------------------
        // Destroy staging buffer
        // --------------------------------------------------------

        vkDestroyBuffer(
            device,
            stagingBuffer,
            allocator);

        vkFreeMemory(
            device,
            stagingMemory,
            allocator);
    }

    void CreateUniformBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBuffer &buffer,
        VkDeviceMemory &memory,
        void **mapped,
        const VkAllocationCallbacks *allocator)
    {
        CreateBuffer(
            device,
            physicalDevice,
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buffer,
            memory,
            allocator);

        if (vkMapMemory(
                device,
                memory,
                0,
                size,
                0,
                mapped) != VK_SUCCESS)
        {
            vkDestroyBuffer(
                device,
                buffer,
                allocator);

            vkFreeMemory(
                device,
                memory,
                allocator);

            buffer = VK_NULL_HANDLE;
            memory = VK_NULL_HANDLE;

            throw std::runtime_error(
                "Failed to map uniform buffer!");
        }
    }
    
    void DestroyBuffer(
        VkDevice device,
        Buffer &buffer,
        const VkAllocationCallbacks *allocator)
    {
        if (buffer.buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(
                device,
                buffer.buffer,
                allocator);

            buffer.buffer = VK_NULL_HANDLE;
        }

        if (buffer.memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(
                device,
                buffer.memory,
                allocator);

            buffer.memory = VK_NULL_HANDLE;
        }
    }
}