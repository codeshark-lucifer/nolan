#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>

namespace OLIA_ENGINE
{

    struct PlatformDesc
    {
        uint32_t width = 956;
        uint32_t height = 540;
        uint32_t minWidth = 1024;
        uint32_t minHeight = 640;

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
        void WaitIfIdle(bool hasActiveWork, bool powerSavingEnabled);
        void MarkActivity();

        bool IsRunning()
        {
            this->PollEvents();
            return m_running;
        }

        bool IsFocused() const { return m_focused; }
        bool IsDropHovering() const { return m_dropHovering; }

        std::vector<std::string> ConsumeDroppedFiles()
        {
            std::vector<std::string> files;
            files.swap(m_droppedFiles);
            return files;
        }

        SDL_Window *GetWindow() const
        {
            return m_window;
        }

    private:
        SDL_Window *m_window = nullptr;
        bool m_running = false;
        bool m_focused = true;
        bool m_dropHovering = false;
        uint64_t m_lastActivityTick = 0;
        std::vector<std::string> m_droppedFiles;
    };

} // namespace OLIA_ENGINE