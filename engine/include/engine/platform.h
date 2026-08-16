#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

namespace OLIA_ENGINE
{

    struct PlatformDesc
    {
        uint32_t width = 956;
        uint32_t height = 540;

        const char *app_name = "olia";
        
        bool resizable = true;
        bool vulkan = true;
    };

    class Platform
    {
    public:
        Platform() = default;
        ~Platform();

        Platform(const Platform &) = delete;
        Platform &operator=(const Platform &) = delete;

        bool Init(const PlatformDesc &desc);
        void Shutdown();

        void PollEvents();

        bool IsRunning()
        {
            this->PollEvents();
            return m_running;
        }

        SDL_Window *GetWindow() const
        {
            return m_window;
        }

    private:
        SDL_Window *m_window = nullptr;
        bool m_running = false;
    };

} // namespace OLIA_ENGINE