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
    typedef struct
    {
        Platform platform;
        VKContext vk_ctx;
        World world;
    } EngineContext;

    extern std::unique_ptr<EngineContext> g_engine_context;

    using SetupFn = void (*)();
    using LoopFn = void (*)();
    extern "C"
    {
        bool engine_init();
        void engine_clean();
        OLIA_API void Run(SetupFn setup, LoopFn loop);
    }
} // namespace OLIA_ENGINE
