#pragma once
#include "./export.h"
#include "./platform.h"
#include "vulkan/vk_context.h"
#include "core/ecs/world.hpp"

#include <memory>

extern OLIA_API std::vector<OLIA_ENGINE::GraphicsPipeline> *graphics;
extern OLIA_API std::vector<VkDescriptorSet> *g_descriptorSet;
namespace OLIA_ENGINE
{
    struct UITexture
    {
        uint64_t id = 0;
        int width = 0;
        int height = 0;
    };

    struct EngineContext
    {
        Platform platform;
        VKContext vk_ctx;
        World world;
        bool powerSaving = true;
        bool activeFrameRequested = false;
    };

    extern std::unique_ptr<EngineContext> g_engine_context;

    using SetupFn = void (*)();
    using LoopFn = void (*)();
    extern "C"
    {
        bool engine_init();
        void engine_clean();
        OLIA_API void Run(SetupFn setup, LoopFn loop);
    }

    OLIA_API void RequestActiveFrame();
    OLIA_API void SetPowerSaving(bool enabled);
    OLIA_API void SetVSync(bool enabled);
    OLIA_API bool IsFileDropHovering();
    OLIA_API uint32_t PollDroppedFileCount();
    OLIA_API const char* GetDroppedFileAt(uint32_t index);
    OLIA_API void ClearDroppedFiles();
    OLIA_API UITexture LoadUITextureFromFile(const char* path);
    OLIA_API UITexture LoadUITextureFromMemory(const void* data, size_t size);
    OLIA_API void UnloadUITexture(uint64_t textureId);
} // namespace OLIA_ENGINE
