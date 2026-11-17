#include "vulkan/commands.hpp"
#include <stdexcept>

namespace OLIA_ENGINE
{
    bool CreateCommandPoolAndBuffer(VKContext &ctx)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily;

        if (vkCreateCommandPool(ctx.device, &poolInfo, ctx.allocator.callbacks(), &ctx.commandPool) != VK_SUCCESS)
            return false;

        ctx.commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(ctx.commandBuffers.size());

        return vkAllocateCommandBuffers(ctx.device, &allocInfo, ctx.commandBuffers.data()) == VK_SUCCESS;
    }

    void RecordCommandBuffer(VKContext &ctx)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(ctx.commandBuffers[ctx.currentFrame], &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin recording command buffer!");
    }

    bool CreateSyncObjects(VKContext &ctx)
    {
        ctx.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        ctx.renderFinishedSemaphores.resize(ctx.swapchainImages.size());
        ctx.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(ctx.device, &semaphoreInfo, ctx.allocator.callbacks(), &ctx.imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(ctx.device, &fenceInfo, ctx.allocator.callbacks(), &ctx.inFlightFences[i]) != VK_SUCCESS)
                return false;
        }
        for (size_t i = 0; i < ctx.renderFinishedSemaphores.size(); i++)
        {
            if (vkCreateSemaphore(ctx.device, &semaphoreInfo, ctx.allocator.callbacks(), &ctx.renderFinishedSemaphores[i]) != VK_SUCCESS)
                return false;
        }
        return true;
    }
}