#pragma once

#include "vulkan/vk_context.h"

namespace OLIA_ENGINE
{

    SwapchainSupport QuerySwapchainSupport(
        VKContext &ctx);

    VkSurfaceFormatKHR ChooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &formats);

    VkPresentModeKHR ChoosePresentMode(
        const std::vector<VkPresentModeKHR> &modes);

    VkExtent2D ChooseSwapExtent(
        const VkSurfaceCapabilitiesKHR &capabilities,
        SDL_Window &window);

    bool CreateSwapchain(
        VKContext &ctx,
        SDL_Window &window);

    bool GetSwapchainImages(
        VKContext &ctx);

    bool CreateSwapchainImageViews(
        VKContext &ctx);

    void DestroySwapchain(
        VKContext &ctx);

}