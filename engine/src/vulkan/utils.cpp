#include "vulkan/utils.hpp"

#include <cstring>
#include <cstdio>
#include <fstream>

namespace OLIA_ENGINE
{
    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation"};

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount = 0;
        VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        if (result != VK_SUCCESS)
            return false;

        std::vector<VkLayerProperties> availableLayers(layerCount);
        result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        if (result != VK_SUCCESS)
            return false;

        for (const char *layerName : validationLayers)
        {
            bool found = false;
            for (const auto &layerProperties : availableLayers)
            {
                if (std::strcmp(layerName, layerProperties.layerName) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }

        return true;
    }

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, const VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);
        // 2. Copy buffer
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
    }

    void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
    }

    void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);

        // End and submit AFTER recording the copy command
        endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
    }

    std::vector<char> ReadFile(const std::string &filename)
    {
        // Open file at the end (ios::ate) to quickly get file size, in binary mode
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            std::printf("[FILE IO] Failed to open file: %s\n", filename.c_str());
            return {};
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

}