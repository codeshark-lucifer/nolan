#include "vulkan/debugger.hpp"

#include <cstdio>

namespace OLIA_ENGINE
{

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    (void)pUserData;

    const char* severity = "UNKNOWN";

    if (messageSeverity &
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        severity = "ERROR";
    }
    else if (messageSeverity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        severity = "WARNING";
    }
    else if (messageSeverity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        severity = "INFO";
    }
    else if (messageSeverity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        severity = "VERBOSE";
    }

    const char* type = "UNKNOWN";

    if (messageType &
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
    {
        type = "GENERAL";
    }
    else if (messageType &
             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
    {
        type = "VALIDATION";
    }
    else if (messageType &
             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
        type = "PERFORMANCE";
    }

    std::printf(
        "\n"
        "========== VULKAN DEBUG ==========\n"
        "[%s] [%s]\n"
        "%s\n"
        "===================================\n\n",
        severity,
        type,
        pCallbackData->pMessage
    );

    return VK_FALSE;
}


VkDebugUtilsMessengerCreateInfoEXT
VK_DebuggerCreateInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};

    createInfo.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback =
        debugCallback;

    return createInfo;
}


bool VK_DebuggerInit(
    VkInstance instance,
    VkDebugUtilsMessengerEXT& debugMessenger,
    const VkAllocationCallbacks* allocator)
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo =
        VK_DebuggerCreateInfo();

    auto function =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(
                instance,
                "vkCreateDebugUtilsMessengerEXT"
            )
        );

    if (function == nullptr)
    {
        std::printf(
            "Failed to load vkCreateDebugUtilsMessengerEXT!\n"
        );

        return false;
    }

    VkResult result = function(
        instance,
        &createInfo,
        allocator,
        &debugMessenger
    );

    if (result != VK_SUCCESS)
    {
        std::printf(
            "Failed to create Vulkan debug messenger! Error: %d\n",
            result
        );

        debugMessenger = VK_NULL_HANDLE;

        return false;
    }

    // std::printf(
    //     "Vulkan debug messenger created.\n"
    // );

    return true;
}


void VK_DebuggerClean(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* allocator)
{
    if (debugMessenger == VK_NULL_HANDLE)
        return;

    auto function =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(
                instance,
                "vkDestroyDebugUtilsMessengerEXT"
            )
        );

    if (function != nullptr)
    {
        function(
            instance,
            debugMessenger,
            allocator
        );
    }
}

} // namespace OLIA_ENGINE