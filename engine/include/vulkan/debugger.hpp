#pragma once

#include <vulkan/vulkan.h>

namespace OLIA_ENGINE
{

VkDebugUtilsMessengerCreateInfoEXT
VK_DebuggerCreateInfo();

bool VK_DebuggerInit(
    VkInstance instance,
    VkDebugUtilsMessengerEXT& debugMessenger,
    const VkAllocationCallbacks* allocator
);

void VK_DebuggerClean(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* allocator
);

} // namespace OLIA_ENGINE