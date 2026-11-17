#include "engine/engine.h"
#include "engine/platform.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <assert.h>

#include "vulkan/vk_context.h"
#include "vulkan/textures.hpp"
#include "vulkan/buffers.hpp"
#include "vulkan/utils.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <stb/stb_image.h>

namespace OLIA_ENGINE
{
    void Run(SetupFn setup, LoopFn loop)
    {
        assert(engine_init() && "Failed to Initialize Engine");

        // Synchronize ImGui Context within the Engine DLL
        if (ImGui::GetCurrentContext() == nullptr)
        {
            ImGui::SetCurrentContext(GetImguiContext());
        }

        setup();

        VK_Setup(g_engine_context->vk_ctx, g_engine_context->world);
        while (g_engine_context->platform.IsRunning())
        {
            g_engine_context->activeFrameRequested = false;

            // 1. Start ImGui Frame
            VK_NewFrameImgui();

            // 2. User application logic & ImGui UI definitions
            loop();

            // 3. End ImGui CPU Frame
            VK_EndFrameImgui();

            // 4. Render Vulkan Frame
            VK_DrawFrame(g_engine_context->vk_ctx, *g_engine_context->platform.GetWindow(), g_engine_context->world);

            // 5. Smart idle wait to save CPU/GPU when no UI/download activity is happening
            g_engine_context->platform.WaitIfIdle(
                g_engine_context->activeFrameRequested,
                g_engine_context->powerSaving);
        }
        engine_clean();
    }

    bool engine_init()
    {
        g_engine_context = std::make_unique<EngineContext>();

        world = &g_engine_context->world;
        graphics = &g_engine_context->vk_ctx.graphics;
        g_descriptorSet = &g_engine_context->vk_ctx.descriptorSets;
        if (!g_engine_context->platform.Init({.width = 1280,
                                              .height = 760,
                                              .minWidth = 1024,
                                              .minHeight = 640,
                                              .app_name = "Nolan Minecraft Launcher",
                                              .resizable = true,
                                              .vulkan = true}))
            return false;
        if (!VK_Init(g_engine_context->vk_ctx, *g_engine_context->platform.GetWindow()))
            return false;

        return true;
    }

    void engine_clean()
    {
        if (g_engine_context)
        {
            VK_Clean(g_engine_context->vk_ctx, g_engine_context->world);
            g_engine_context.reset();
        }
    }

    void RequestActiveFrame()
    {
        if (g_engine_context)
        {
            g_engine_context->activeFrameRequested = true;
        }
    }

    void SetPowerSaving(bool enabled)
    {
        if (g_engine_context)
        {
            g_engine_context->powerSaving = enabled;
        }
    }

    void SetVSync(bool enabled)
    {
        if (g_engine_context && g_engine_context->vk_ctx.vsync != enabled)
        {
            g_engine_context->vk_ctx.vsync = enabled;
            g_engine_context->vk_ctx.resized = true;
        }
    }

    bool IsFileDropHovering()
    {
        return g_engine_context ? g_engine_context->platform.IsDropHovering() : false;
    }

    static std::vector<std::string> g_polledDroppedFiles;

    uint32_t PollDroppedFileCount()
    {
        if (!g_engine_context)
            return 0;
        g_polledDroppedFiles = g_engine_context->platform.ConsumeDroppedFiles();
        return static_cast<uint32_t>(g_polledDroppedFiles.size());
    }

    const char* GetDroppedFileAt(uint32_t index)
    {
        if (index >= g_polledDroppedFiles.size())
            return "";
        return g_polledDroppedFiles[index].c_str();
    }

    void ClearDroppedFiles()
    {
        g_polledDroppedFiles.clear();
    }

    static UITexture CreateUITextureFromRGBA(const unsigned char* pixels, int width, int height)
    {
        UITexture out{0, 0, 0};
        if (!g_engine_context || !pixels || width <= 0 || height <= 0)
            return out;

        VKContext& ctx = g_engine_context->vk_ctx;
        VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        CreateBuffer(
            ctx.device, ctx.physicalDevice, imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingMemory, ctx.allocator.callbacks());

        void* mapped = nullptr;
        vkMapMemory(ctx.device, stagingMemory, 0, imageSize, 0, &mapped);
        memcpy(mapped, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(ctx.device, stagingMemory);

        UITextureRecord rec{};
        rec.width = width;
        rec.height = height;

        CreateImage(
            ctx.device, ctx.physicalDevice,
            static_cast<uint32_t>(width), static_cast<uint32_t>(height),
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            rec.image, rec.memory, VK_SAMPLE_COUNT_1_BIT, ctx.allocator.callbacks());

        transitionImageLayout(
            ctx.device, ctx.commandPool, ctx.graphicsQueue,
            rec.image, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(
            ctx.device, ctx.commandPool, ctx.graphicsQueue,
            stagingBuffer, rec.image,
            static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        transitionImageLayout(
            ctx.device, ctx.commandPool, ctx.graphicsQueue,
            rec.image, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(ctx.device, stagingBuffer, ctx.allocator.callbacks());
        vkFreeMemory(ctx.device, stagingMemory, ctx.allocator.callbacks());

        rec.imageView = CreateImageView(
            ctx.device, rec.image, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_ASPECT_COLOR_BIT, ctx.allocator.callbacks());
        rec.descriptorSet = ImGui_ImplVulkan_AddTexture(
            rec.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        rec.id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(rec.descriptorSet));

        ctx.uiTextures.push_back(rec);
        out.id = rec.id;
        out.width = width;
        out.height = height;
        return out;
    }

    UITexture LoadUITextureFromFile(const char* path)
    {
        if (!path || !path[0])
            return {0, 0, 0};

        stbi_set_flip_vertically_on_load(false);
        int w = 0, h = 0, ch = 0;
        stbi_uc* pixels = stbi_load(path, &w, &h, &ch, STBI_rgb_alpha);
        if (!pixels)
            return {0, 0, 0};

        UITexture tex = CreateUITextureFromRGBA(pixels, w, h);
        stbi_image_free(pixels);
        return tex;
    }

    UITexture LoadUITextureFromMemory(const void* data, size_t size)
    {
        if (!data || size == 0)
            return {0, 0, 0};

        stbi_set_flip_vertically_on_load(false);
        int w = 0, h = 0, ch = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            static_cast<const stbi_uc*>(data), static_cast<int>(size), &w, &h, &ch, STBI_rgb_alpha);
        if (!pixels)
            return {0, 0, 0};

        UITexture tex = CreateUITextureFromRGBA(pixels, w, h);
        stbi_image_free(pixels);
        return tex;
    }

    void UnloadUITexture(uint64_t textureId)
    {
        if (!g_engine_context || textureId == 0)
            return;

        VKContext& ctx = g_engine_context->vk_ctx;
        for (auto it = ctx.uiTextures.begin(); it != ctx.uiTextures.end(); ++it)
        {
            if (it->id == textureId)
            {
                vkDeviceWaitIdle(ctx.device);
                if (it->descriptorSet != VK_NULL_HANDLE)
                    ImGui_ImplVulkan_RemoveTexture(it->descriptorSet);
                if (it->imageView != VK_NULL_HANDLE)
                    vkDestroyImageView(ctx.device, it->imageView, ctx.allocator.callbacks());
                if (it->image != VK_NULL_HANDLE)
                    vkDestroyImage(ctx.device, it->image, ctx.allocator.callbacks());
                if (it->memory != VK_NULL_HANDLE)
                    vkFreeMemory(ctx.device, it->memory, ctx.allocator.callbacks());
                ctx.uiTextures.erase(it);
                return;
            }
        }
    }
} // namespace OLIA_ENGINE