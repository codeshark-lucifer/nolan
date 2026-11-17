#include "vulkan/swapchain.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace OLIA_ENGINE
{

    SwapchainSupport QuerySwapchainSupport(
        VKContext &ctx)
    {
        SwapchainSupport support{};

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            ctx.physicalDevice,
            ctx.surface,
            &support.capabilities);

        // ------------------------------------------------------------
        // Surface formats
        // ------------------------------------------------------------

        uint32_t formatCount = 0;

        vkGetPhysicalDeviceSurfaceFormatsKHR(
            ctx.physicalDevice,
            ctx.surface,
            &formatCount,
            nullptr);

        if (formatCount > 0)
        {
            support.formats.resize(
                formatCount);

            vkGetPhysicalDeviceSurfaceFormatsKHR(
                ctx.physicalDevice,
                ctx.surface,
                &formatCount,
                support.formats.data());
        }

        // ------------------------------------------------------------
        // Present modes
        // ------------------------------------------------------------

        uint32_t presentModeCount = 0;

        vkGetPhysicalDeviceSurfacePresentModesKHR(
            ctx.physicalDevice,
            ctx.surface,
            &presentModeCount,
            nullptr);

        if (presentModeCount > 0)
        {
            support.presentModes.resize(
                presentModeCount);

            vkGetPhysicalDeviceSurfacePresentModesKHR(
                ctx.physicalDevice,
                ctx.surface,
                &presentModeCount,
                support.presentModes.data());
        }

        return support;
    }

    VkSurfaceFormatKHR ChooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &formats)
    {
        for (const auto &format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format;
            }
        }

        return formats[0];
    }

    VkPresentModeKHR ChoosePresentMode(
        const std::vector<VkPresentModeKHR> &modes,
        bool vsync)
    {
        if (vsync)
        {
            // FIFO is VSync and guaranteed to exist without burning idle GPU.
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        for (const auto &mode : modes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D ChooseSwapExtent(
        const VkSurfaceCapabilitiesKHR &capabilities,
        SDL_Window &window)
    {
        // Make sure extent width is valid and non-zero
        if (capabilities.currentExtent.width != UINT32_MAX && capabilities.currentExtent.width > 0)
        {
            return capabilities.currentExtent;
        }

        int width = 0;
        int height = 0;

        SDL_GetWindowSizeInPixels(
            &window,
            &width,
            &height);

        VkExtent2D extent{};

        extent.width = static_cast<uint32_t>(width);
        extent.height = static_cast<uint32_t>(height);

        extent.width = std::clamp(
            extent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);

        extent.height = std::clamp(
            extent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);

        return extent;
    }

    bool CreateSwapchain(
        VKContext &ctx,
        SDL_Window &window)
    {
        SwapchainSupport support =
            QuerySwapchainSupport(ctx);

        if (support.formats.empty())
        {
            std::printf(
                "No Vulkan surface formats available.\n");

            return false;
        }

        if (support.presentModes.empty())
        {
            std::printf(
                "No Vulkan present modes available.\n");

            return false;
        }

        // ------------------------------------------------------------
        // Choose format
        // ------------------------------------------------------------

        VkSurfaceFormatKHR surfaceFormat =
            ChooseSurfaceFormat(
                support.formats);

        // ------------------------------------------------------------
        // Choose present mode
        // ------------------------------------------------------------

        VkPresentModeKHR presentMode =
            ChoosePresentMode(
                support.presentModes,
                ctx.vsync);

        // ------------------------------------------------------------
        // Choose extent
        // ------------------------------------------------------------

        VkExtent2D extent =
            ChooseSwapExtent(
                support.capabilities,
                window);

        // ------------------------------------------------------------
        // Image count
        // ------------------------------------------------------------

        uint32_t imageCount =
            support.capabilities.minImageCount + 1;

        if (support.capabilities.maxImageCount > 0 &&
            imageCount >
                support.capabilities.maxImageCount)
        {
            imageCount =
                support.capabilities.maxImageCount;
        }

        // ------------------------------------------------------------
        // Create info
        // ------------------------------------------------------------

        VkSwapchainCreateInfoKHR createInfo{};

        createInfo.sType =
            VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

        createInfo.surface =
            ctx.surface;

        createInfo.minImageCount =
            imageCount;

        createInfo.imageFormat =
            surfaceFormat.format;

        createInfo.imageColorSpace =
            surfaceFormat.colorSpace;

        createInfo.imageExtent =
            extent;

        createInfo.imageArrayLayers =
            1;

        createInfo.imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // ------------------------------------------------------------
        // Queue sharing
        // ------------------------------------------------------------

        uint32_t queueFamilyIndices[] =
            {
                ctx.graphicsQueueFamily,
                ctx.presentQueueFamily};

        if (ctx.graphicsQueueFamily !=
            ctx.presentQueueFamily)
        {
            createInfo.imageSharingMode =
                VK_SHARING_MODE_CONCURRENT;

            createInfo.queueFamilyIndexCount =
                2;

            createInfo.pQueueFamilyIndices =
                queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode =
                VK_SHARING_MODE_EXCLUSIVE;

            createInfo.queueFamilyIndexCount =
                0;

            createInfo.pQueueFamilyIndices =
                nullptr;
        }

        // ------------------------------------------------------------
        // Other settings
        // ------------------------------------------------------------

        createInfo.preTransform =
            support.capabilities.currentTransform;

        createInfo.compositeAlpha =
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode =
            presentMode;

        createInfo.clipped =
            VK_TRUE;

        createInfo.oldSwapchain =
            VK_NULL_HANDLE;

        // ------------------------------------------------------------
        // Create swapchain
        // ------------------------------------------------------------

        VkResult result =
            vkCreateSwapchainKHR(
                ctx.device,
                &createInfo,
                ctx.allocator.callbacks(),
                &ctx.swapchain);

        if (result != VK_SUCCESS)
        {
            std::printf(
                "[VULKAN] Failed to create swapchain: %d\n",
                static_cast<int>(result));

            return false;
        }

        ctx.swapchainFormat =
            surfaceFormat.format;

        ctx.swapchainColorSpace =
            surfaceFormat.colorSpace;

        ctx.swapchainExtent =
            extent;

        std::printf(
            " - swapchain\n");

        std::printf(
            "   format: %d\n",
            static_cast<int>(
                ctx.swapchainFormat));

        std::printf(
            "   extent: %ux%u\n",
            ctx.swapchainExtent.width,
            ctx.swapchainExtent.height);

        return true;
    }

    bool GetSwapchainImages(
        VKContext &ctx)
    {
        uint32_t imageCount = 0;

        VkResult result =
            vkGetSwapchainImagesKHR(
                ctx.device,
                ctx.swapchain,
                &imageCount,
                nullptr);

        if (result != VK_SUCCESS ||
            imageCount == 0)
        {
            std::printf(
                "[VULKAN] Failed to get swapchain image count.\n");

            return false;
        }

        ctx.swapchainImages.resize(
            imageCount);

        result =
            vkGetSwapchainImagesKHR(
                ctx.device,
                ctx.swapchain,
                &imageCount,
                ctx.swapchainImages.data());

        if (result != VK_SUCCESS)
        {
            std::printf(
                "[VULKAN] Failed to get swapchain images.\n");

            ctx.swapchainImages.clear();

            return false;
        }

        std::printf(
            " - swapchain images: %u\n",
            imageCount);

        return true;
    }

    bool CreateSwapchainImageViews(
        VKContext &ctx)
    {
        ctx.swapchainImageViews.resize(
            ctx.swapchainImages.size());

        for (size_t i = 0;
             i < ctx.swapchainImages.size();
             ++i)
        {
            VkImageViewCreateInfo createInfo{};

            createInfo.sType =
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

            createInfo.image =
                ctx.swapchainImages[i];

            createInfo.viewType =
                VK_IMAGE_VIEW_TYPE_2D;

            createInfo.format =
                ctx.swapchainFormat;

            createInfo.components.r =
                VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.components.g =
                VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.components.b =
                VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.components.a =
                VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;

            createInfo.subresourceRange.baseMipLevel =
                0;

            createInfo.subresourceRange.levelCount =
                1;

            createInfo.subresourceRange.baseArrayLayer =
                0;

            createInfo.subresourceRange.layerCount =
                1;

            VkResult result =
                vkCreateImageView(
                    ctx.device,
                    &createInfo,
                    ctx.allocator.callbacks(),
                    &ctx.swapchainImageViews[i]);

            if (result != VK_SUCCESS)
            {
                std::printf(
                    "[VULKAN] Failed to create swapchain image view %zu\n",
                    i);

                // Destroy views already created.
                for (size_t j = 0; j < i; ++j)
                {
                    vkDestroyImageView(
                        ctx.device,
                        ctx.swapchainImageViews[j],
                        ctx.allocator.callbacks());

                    ctx.swapchainImageViews[j] =
                        VK_NULL_HANDLE;
                }

                ctx.swapchainImageViews.clear();

                return false;
            }
        }

        std::printf(
            " - swapchain image views: %zu\n",
            ctx.swapchainImageViews.size());

        return true;
    }

    void DestroySwapchain(
        VKContext &ctx)
    {
        // ------------------------------------------------------------
        // Image views
        // ------------------------------------------------------------

        for (VkImageView imageView :
             ctx.swapchainImageViews)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(
                    ctx.device,
                    imageView,
                    ctx.allocator.callbacks());
            }
        }

        ctx.swapchainImageViews.clear();

        // ------------------------------------------------------------
        // Images
        //
        // Swapchain images are owned by Vulkan.
        // We DO NOT destroy them manually.
        // ------------------------------------------------------------

        ctx.swapchainImages.clear();

        // ------------------------------------------------------------
        // Swapchain
        // ------------------------------------------------------------

        if (ctx.swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(
                ctx.device,
                ctx.swapchain,
                ctx.allocator.callbacks());

            ctx.swapchain =
                VK_NULL_HANDLE;
        }

        ctx.swapchainFormat =
            VK_FORMAT_UNDEFINED;

        ctx.swapchainExtent =
            {};

        ctx.swapchainColorSpace =
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

}