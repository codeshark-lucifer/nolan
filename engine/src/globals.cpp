#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "engine/platform.h"
#include "engine/engine.h"
#include "vulkan/vk_context.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <memory>
#include <cstdio>

#include "core/ecs/world.hpp"
#include "vulkan/pipeline.hpp"

#include <SDL3/SDL.h>
#include <stb/stb_image.h> // Only include standard stb_image.h
#ifdef _WIN32
#include <shobjidl.h> // For SetCurrentProcessExplicitAppUserModelID
#endif

OLIA_ENGINE::World *world = nullptr;
std::vector<OLIA_ENGINE::GraphicsPipeline> *graphics = nullptr;
std::vector<VkDescriptorSet> *g_descriptorSet = nullptr;

namespace OLIA_ENGINE
{
    std::unique_ptr<EngineContext> g_engine_context = nullptr;

    Platform::~Platform()
    {
        Shutdown();
    }

    bool Platform::Init(const PlatformDesc &desc)
    {
#ifdef _WIN32
    SetCurrentProcessExplicitAppUserModelID(L"Olia.Engine.App");
#endif
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            std::printf("SDL_Init Error: %s\n", SDL_GetError());
            return false;
        }

        SDL_WindowFlags flags = (SDL_WindowFlags)((desc.vulkan ? SDL_WINDOW_VULKAN : 0) |
                                                  (desc.resizable ? SDL_WINDOW_RESIZABLE : 0));

        // 1. Create window
        m_window = SDL_CreateWindow(
            desc.app_name,
            static_cast<int>(desc.width),
            static_cast<int>(desc.height),
            flags);

        if (!m_window)
        {
            std::printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
            SDL_Quit();
            return false;
        }

        // 2. Load PNG using stb_image (forces 4 channels / RGBA)
        int width = 0, height = 0, channels = 0;
        unsigned char *pixels = stbi_load("assets/icon.png", &width, &height, &channels, 4);

        if (pixels)
        {
            // 3. Create SDL3 surface directly from pixel memory
            SDL_Surface *iconSurface = SDL_CreateSurfaceFrom(
                width,
                height,
                SDL_PIXELFORMAT_RGBA32,
                pixels,
                width * 4 // pitch (width * bytes_per_pixel)
            );

            if (iconSurface)
            {
                SDL_SetWindowIcon(m_window, iconSurface);
                SDL_DestroySurface(iconSurface); // Destroys SDL surface wrapper
            }

            stbi_image_free(pixels); // Free raw pixel memory allocated by stb_image
        }
        else
        {
            std::printf("Warning: Failed to load icon image: assets/icon.png\n");
        }

        m_running = true;
        return true;
    }

    void Platform::PollEvents()
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                m_running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (g_engine_context)
                {
                    g_engine_context->vk_ctx.resized = true;
                }
                break;

            default:
                break;
            }
        }
    }

    void Platform::Shutdown()
    {
        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        if (m_running)
        {
            m_running = false;
        }

        SDL_Quit();
    }

} // namespace OLIA_ENGINE