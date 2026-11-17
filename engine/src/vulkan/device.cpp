#include "vulkan/device.hpp"

#include <cstdio>
#include <set>
#include <vector>

namespace OLIA_ENGINE
{

    int ScorePhysicalDevice(
        VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(
            device,
            &properties);

        int score = 0;

        // Prefer discrete GPUs.
        if (properties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += 1000;
        }

        // Integrated GPUs are still valid.
        if (properties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            score += 100;
        }

        return score;
    }

    const char *GetGPUType(VkPhysicalDeviceType type)
    {
        switch (type)
        {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "Other";

        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated";

        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete";

        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual";

        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";

        default:
            return "Unknown";
        }
    }

    const char *GetGPUVendor(uint32_t vendorID)
    {
        switch (vendorID)
        {
        case 0x10DE:
            return "NVIDIA";

        case 0x1002:
            return "AMD";

        case 0x8086:
            return "Intel";

        default:
            return "Unknown";
        }
    }

    QueueFamilyIndices FindQueueFamilies(
        VKContext &ctx,
        VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;

        vkGetPhysicalDeviceQueueFamilyProperties(
            device,
            &queueFamilyCount,
            nullptr);

        std::vector<VkQueueFamilyProperties>
            queueFamilies(queueFamilyCount);

        vkGetPhysicalDeviceQueueFamilyProperties(
            device,
            &queueFamilyCount,
            queueFamilies.data());

        for (uint32_t i = 0;
             i < queueFamilyCount;
             ++i)
        {
            const VkQueueFamilyProperties &
                queueFamily = queueFamilies[i];

            // --------------------------------------------------------
            // Graphics
            // --------------------------------------------------------

            if (queueFamily.queueCount > 0 &&
                (queueFamily.queueFlags &
                 VK_QUEUE_GRAPHICS_BIT))
            {
                if (!indices.graphics.has_value())
                {
                    indices.graphics = i;
                }
            }

            // --------------------------------------------------------
            // Present
            // --------------------------------------------------------

            VkBool32 presentSupport = VK_FALSE;

            VkResult result =
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    device,
                    i,
                    ctx.surface,
                    &presentSupport);

            if (result == VK_SUCCESS &&
                queueFamily.queueCount > 0 &&
                presentSupport)
            {
                if (!indices.present.has_value())
                {
                    indices.present = i;
                }
            }

            if (indices.complete())
            {
                break;
            }
        }

        return indices;
    }

    bool PickPhysicalDevice(
        VKContext &ctx)
    {
        uint32_t deviceCount = 0;

        VkResult result =
            vkEnumeratePhysicalDevices(
                ctx.instance,
                &deviceCount,
                nullptr);

        if (result != VK_SUCCESS ||
            deviceCount == 0)
        {
            std::printf(
                "No Vulkan physical devices found.\n");

            return false;
        }

        std::vector<VkPhysicalDevice>
            devices(deviceCount);

        result =
            vkEnumeratePhysicalDevices(
                ctx.instance,
                &deviceCount,
                devices.data());

        if (result != VK_SUCCESS)
        {
            std::printf(
                "Failed to enumerate Vulkan physical devices.\n");

            return false;
        }

        std::printf(
            " - physical devices: %u\n",
            deviceCount);

        VkPhysicalDevice bestDevice =
            VK_NULL_HANDLE;

        int bestScore = -1;

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties{};

            vkGetPhysicalDeviceProperties(
                device,
                &properties);

            QueueFamilyIndices queues =
                FindQueueFamilies(
                    ctx,
                    device);

            int score =
                ScorePhysicalDevice(device);

            std::printf(
                "  GPU: %s\n",
                properties.deviceName);

            std::printf(
                "  Vendor: %s\n",
                GetGPUVendor(
                    properties.vendorID));

            std::printf(
                "  Type: %s\n",
                GetGPUType(
                    properties.deviceType));

            std::printf(
                "  Score: %d\n",
                score);

            if (!queues.complete())
            {
                std::printf(
                    "  -> rejected: missing required queues\n");

                continue;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestDevice = device;
            }
        }

        if (bestDevice == VK_NULL_HANDLE)
        {
            std::printf(
                "No suitable Vulkan GPU found.\n");

            return false;
        }

        ctx.physicalDevice =
            bestDevice;

        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(
            ctx.physicalDevice,
            &properties);

        QueueFamilyIndices indices =
            FindQueueFamilies(
                ctx,
                ctx.physicalDevice);

        ctx.graphicsQueueFamily =
            indices.graphics.value();

        ctx.presentQueueFamily =
            indices.present.value();

        std::printf(
            "  -> selected: %s\n",
            properties.deviceName);

        std::printf(
            "  graphics queue family: %u\n",
            ctx.graphicsQueueFamily);

        std::printf(
            "  present queue family: %u\n",
            ctx.presentQueueFamily);

        return true;
    }

    bool CreateLogicalDevice(
        VKContext &ctx)
    {
        QueueFamilyIndices indices =
            FindQueueFamilies(
                ctx,
                ctx.physicalDevice);

        if (!indices.complete())
        {
            std::printf(
                "[VULKAN] Required queue families not found.\n");

            return false;
        }

        ctx.graphicsQueueFamily =
            indices.graphics.value();

        ctx.presentQueueFamily =
            indices.present.value();

        // ------------------------------------------------------------
        // Unique queue families
        // ------------------------------------------------------------

        std::set<uint32_t>
            uniqueQueueFamilies =
                {
                    ctx.graphicsQueueFamily,
                    ctx.presentQueueFamily};

        // ------------------------------------------------------------
        // Queue creation
        // ------------------------------------------------------------

        float queuePriority = 1.0f;

        std::vector<VkDeviceQueueCreateInfo>
            queueCreateInfos;

        for (uint32_t queueFamily :
             uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};

            queueCreateInfo.sType =
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

            queueCreateInfo.queueFamilyIndex =
                queueFamily;

            queueCreateInfo.queueCount = 1;

            queueCreateInfo.pQueuePriorities =
                &queuePriority;

            queueCreateInfos.push_back(
                queueCreateInfo);
        }

        // ------------------------------------------------------------
        // Device features
        // ------------------------------------------------------------

        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(ctx.physicalDevice, &supportedFeatures);

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = supportedFeatures.samplerAnisotropy;

        // ------------------------------------------------------------
        // Device extension support
        // ------------------------------------------------------------

        const char *deviceExtensions[] =
            {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };

        // ------------------------------------------------------------
        // Device create info
        // ------------------------------------------------------------

        VkDeviceCreateInfo createInfo{};

        createInfo.sType =
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount =
            static_cast<uint32_t>(
                queueCreateInfos.size());

        createInfo.pQueueCreateInfos =
            queueCreateInfos.data();

        createInfo.enabledExtensionCount =
            static_cast<uint32_t>(
                sizeof(deviceExtensions) /
                sizeof(deviceExtensions[0]));

        createInfo.ppEnabledExtensionNames =
            deviceExtensions;

        createInfo.pEnabledFeatures =
            &deviceFeatures;

        // ------------------------------------------------------------
        // Create logical device
        // ------------------------------------------------------------

        VkResult result =
            vkCreateDevice(
                ctx.physicalDevice,
                &createInfo,
                ctx.allocator.callbacks(),
                &ctx.device);

        if (result != VK_SUCCESS)
        {
            std::printf(
                "[VULKAN] Failed to create logical device: %d\n",
                static_cast<int>(result));

            return false;
        }

        // ------------------------------------------------------------
        // Get queues
        // ------------------------------------------------------------

        vkGetDeviceQueue(
            ctx.device,
            ctx.graphicsQueueFamily,
            0,
            &ctx.graphicsQueue);

        vkGetDeviceQueue(
            ctx.device,
            ctx.presentQueueFamily,
            0,
            &ctx.presentQueue);

        std::printf(
            " - logical device\n");

        std::printf(
            " - graphics queue\n");

        std::printf(
            " - present queue\n");

        return true;
    }

}