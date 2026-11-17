#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

#include "./allocator.hpp"
#include "./pipeline.hpp"

#include "core/ecs/world.hpp"

#include "core/components/camera.h"
#include "core/components/transform.h"

#include "engine/export.h"
#include <imgui.h>

namespace OLIA_ENGINE
{
    const int MAX_FRAMES_IN_FLIGHT = 2;

    struct PostProcessPipeline
    {
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    };

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool complete() const { return graphics.has_value() && present.has_value(); }
    };

    struct SwapchainSupport
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct UniformBuffers
    {
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceMemory> memories;
        std::vector<void *> mappeds;

        VkImage textureImage = VK_NULL_HANDLE;
        VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
        VkImageView textureImageView = VK_NULL_HANDLE;
        VkSampler textureSampler = VK_NULL_HANDLE;
    };

    struct UITextureRecord
    {
        uint64_t id = 0;
        int width = 0;
        int height = 0;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };

    struct VKContext
    {
        // --- Core Handles ---
        VkInstance instance = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint32_t graphicsQueueFamily = 0;
        uint32_t presentQueueFamily = 0;

        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;

        // --- Swapchain ---
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkExtent2D swapchainExtent{};
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;

        // --- MSAA & Depth ---
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        VkImage colorImage = VK_NULL_HANDLE;
        VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
        VkImageView colorImageView = VK_NULL_HANDLE;

        VkImage depthImage = VK_NULL_HANDLE;
        VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        // --- Offscreen & Post-Process ---
        VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
        VkImage offscreenColorImage = VK_NULL_HANDLE;
        VkDeviceMemory offscreenColorImageMemory = VK_NULL_HANDLE;
        VkImageView offscreenColorImageView = VK_NULL_HANDLE;
        VkSampler offscreenSampler = VK_NULL_HANDLE;
        VkFramebuffer offscreenFramebuffer = VK_NULL_HANDLE;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> swapchainFramebuffers;

        std::vector<GraphicsPipeline> graphics;
        PostProcessPipeline postProcessPipeline;

        VkPipelineCache pipelineCache = VK_NULL_HANDLE;

        VkDescriptorPool postProcessDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet postProcessDescriptorSet = VK_NULL_HANDLE;

        // --- Commands & Sync ---
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;

        Allocator allocator;
        bool resized = false;
        bool vsync = true;
        uint32_t currentFrame = 0;

        UniformBuffers uniforms;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> descriptorSets;

        VkDescriptorPool imguiPool = VK_NULL_HANDLE;
        std::vector<UITextureRecord> uiTextures;
    };

    bool CreateInstance(VKContext &ctx);
    bool CreateSurface(VKContext &ctx, SDL_Window &window);

    void VK_Setup(VKContext &ctx, World &world);
    bool VK_Init(VKContext &ctx, SDL_Window &window);
    void VK_Clean(VKContext &ctx, World &world);

    OLIA_API ImGuiContext* GetImguiContext();

    void VK_NewFrameImgui();
    void VK_EndFrameImgui();
    void VK_InitImgui(VKContext &ctx, SDL_Window &window);
    void VK_RenderImgui(VKContext &ctx);
    
    bool RecreateSwapchain(VKContext &ctx, SDL_Window &window);
    void CleanupSwapchain(VKContext &ctx);

    bool VK_DrawFrame(VKContext &ctx, SDL_Window &window, World &world);

    void UpdateUniformBuffer(VKContext &ctx, const Camera &camera, const Transform &camTrans);
}