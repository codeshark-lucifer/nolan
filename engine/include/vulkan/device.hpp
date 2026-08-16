#pragma once

#include "vulkan/vk_context.h"
#include <vulkan/vulkan.h>

namespace OLIA_ENGINE
{

    const char *GetGPUType(VkPhysicalDeviceType type);
    const char *GetGPUVendor(uint32_t vendorID);

    bool PickPhysicalDevice(VKContext &ctx);
    bool CreateLogicalDevice(VKContext &ctx);

}