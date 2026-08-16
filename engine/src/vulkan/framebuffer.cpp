#include "vulkan/framebuffer.hpp"

namespace OLIA_ENGINE
{
    bool CreateFramebuffers(VKContext &ctx)
    {
        ctx.swapchainFramebuffers.resize(ctx.swapchainImageViews.size());

        for (size_t i = 0; i < ctx.swapchainImageViews.size(); i++)
        {
            VkImageView attachments[] = {ctx.swapchainImageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebufferInfo.renderPass = ctx.renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = ctx.swapchainExtent.width;
            framebufferInfo.height = ctx.swapchainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(ctx.device, &framebufferInfo, ctx.allocator.callbacks(), &ctx.swapchainFramebuffers[i]) != VK_SUCCESS)
                return false;
        }
        return true;
    }
}