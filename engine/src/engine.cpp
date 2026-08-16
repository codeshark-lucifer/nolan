#include "engine/engine.h"
#include "engine/platform.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <assert.h>

#include "vulkan/vk_context.h"
#include <imgui.h>

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
            // 1. Start ImGui Frame
            VK_NewFrameImgui();

            // 2. User application logic & ImGui UI definitions
            loop();

            // 3. End ImGui CPU Frame
            VK_EndFrameImgui();

            // 4. Render Vulkan Frame
            VK_DrawFrame(g_engine_context->vk_ctx, *g_engine_context->platform.GetWindow(), g_engine_context->world);
        }
        engine_clean();
    }

    bool engine_init()
    {
        g_engine_context = std::make_unique<EngineContext>();

        world = &g_engine_context->world;
        graphics = &g_engine_context->vk_ctx.graphics;
        g_descriptorSet = &g_engine_context->vk_ctx.descriptorSets;
        if (!g_engine_context->platform.Init({.width = 956,
                                              .height = 540,
                                              .app_name = "olia",
                                              .resizable = false,
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
} // namespace OLIA_ENGINE