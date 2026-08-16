#include "app.hpp"
#include "engine/engine.h"
#include "vulkan/vk_context.h"
#include "core/components/camera.h"
#include "core/components/transform.h"
#include "core/components/mesh-renderer.h"
#include "core/components/material.h"
#include <imgui.h>
#include "launcher/launcher.hpp"

struct LauncherUI
{
    char username[64] = "Steve";
    int selected_version = 0;
    std::vector<VersionEntry> versions;
    bool isOnlineMode = false;
    int ramAllocation = 4;
    
    std::string statusText = "Ready";
    std::atomic<float> downloadProgress{0.0f};
    std::atomic<bool> isBusy{false};
} g_LauncherUI;

// --- Custom Modern Theme ---
void ApplyLauncherTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;       // Seamless edge-to-edge docking
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowPadding = ImVec2(16, 16);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.12f, 0.14f, 0.17f, 0.98f);
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.23f, 0.28f, 0.50f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.16f, 0.62f, 0.36f, 1.00f); // Emerald Green
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.20f, 0.74f, 0.44f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.12f, 0.52f, 0.30f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.24f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.20f, 0.26f, 0.34f, 1.00f);
    colors[ImGuiCol_PlotHistogram]        = ImVec4(0.16f, 0.62f, 0.36f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.20f, 0.74f, 0.44f, 1.00f);
    colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.16f, 0.62f, 0.36f, 1.00f);
}

// Spinner frames for loading indicator
const char* GetSpinnerChar()
{
    static const char* frames[] = { "|", "/", "-", "\\" };
    int idx = (int)(ImGui::GetTime() * 8.0f) % 4;
    return frames[idx];
}

void RefreshVersionListAsync()
{
    if (g_LauncherUI.isBusy) return;

    g_LauncherUI.isBusy = true;
    g_LauncherUI.statusText = "Fetching version manifest from Mojang...";

    std::thread([]() {
        g_LauncherUI.versions = MinecraftLauncher::FetchVersions();
        g_LauncherUI.selected_version = 0;
        g_LauncherUI.statusText = "Ready";
        g_LauncherUI.isBusy = false;
    }).detach();
}

void setup()
{
    ImGui::SetCurrentContext(OLIA_ENGINE::GetImguiContext());
    ApplyLauncherTheme();

    // Fetch version list asynchronously at startup
    RefreshVersionListAsync();
}

void loop()
{
    // Make ImGui Window stretch 100% to fill the OS Vulkan viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::Begin("Nolan Engine - Minecraft Launcher", nullptr, 
        ImGuiWindowFlags_NoDecoration | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoSavedSettings);

    // --- Header Banner ---
    ImGui::TextColored(ImVec4(0.20f, 0.74f, 0.44f, 1.00f), "NOLAN LAUNCHER");
    ImGui::SameLine();
    ImGui::TextDisabled("| Vulkan Engine Active (FPS: %.1f)", ImGui::GetIO().Framerate);
    ImGui::Separator();
    ImGui::Spacing();

    bool isBusyFrame = g_LauncherUI.isBusy.load();

    if (ImGui::BeginTabBar("LauncherTabs"))
    {
        // TAB 1: PLAY
        if (ImGui::BeginTabItem("Play"))
        {
            ImGui::Spacing();

            // Card 1: Account Configuration
            ImGui::BeginChild("AccountCard", ImVec2(0, 120), true);
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "ACCOUNT CONFIGURATION");
            ImGui::Separator();
            
            ImGui::Checkbox("Use Official Account (Online Mode)", &g_LauncherUI.isOnlineMode);

            if (g_LauncherUI.isOnlineMode)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Online authentication mode selected.");
            }
            else
            {
                ImGui::SetNextItemWidth(260);
                ImGui::InputText("Username", g_LauncherUI.username, IM_ARRAYSIZE(g_LauncherUI.username));
                
                ImGui::SameLine();
                std::string previewUUID = GenerateOfflineUUID(g_LauncherUI.username);
                ImGui::TextDisabled("[UUID: %.8s...]", previewUUID.c_str());
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 2: Version & Hardware Settings
            ImGui::BeginChild("SettingsCard", ImVec2(0, 125), true);
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "GAME & HARDWARE SETTINGS");
            ImGui::Separator();

            // Version Dropdown (Always visible)
            if (g_LauncherUI.versions.empty())
            {
                ImGui::SetNextItemWidth(340);
                ImGui::BeginDisabled();
                if (ImGui::BeginCombo("Version", "Loading versions from Mojang..."))
                {
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
            }
            else
            {
                std::string currentLabel = g_LauncherUI.versions[g_LauncherUI.selected_version].id;
                if (g_LauncherUI.versions[g_LauncherUI.selected_version].isDownloaded)
                {
                    currentLabel += " [Installed]";
                }

                ImGui::SetNextItemWidth(340);
                if (ImGui::BeginCombo("Version", currentLabel.c_str()))
                {
                    for (int i = 0; i < (int)g_LauncherUI.versions.size(); i++)
                    {
                        bool isSelected = (g_LauncherUI.selected_version == i);
                        std::string itemLabel = g_LauncherUI.versions[i].id;
                        if (g_LauncherUI.versions[i].isDownloaded)
                        {
                            itemLabel += " [Installed]";
                        }

                        if (ImGui::Selectable(itemLabel.c_str(), isSelected))
                        {
                            g_LauncherUI.selected_version = i;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::SameLine();
            if (isBusyFrame) ImGui::BeginDisabled();
            if (ImGui::Button("Refresh", ImVec2(90, 0)))
            {
                RefreshVersionListAsync();
            }
            if (isBusyFrame) ImGui::EndDisabled();

            // RAM Allocation
            ImGui::SetNextItemWidth(340);
            ImGui::SliderInt("Memory", &g_LauncherUI.ramAllocation, 2, 16, "%d GB RAM");
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 3: Launch & Status Panel
            ImGui::BeginChild("LaunchCard", ImVec2(0, 0), true);
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "LAUNCHER STATUS");
            ImGui::Separator();

            if (isBusyFrame)
            {
                // Loading Overlay & Progress
                float prog = g_LauncherUI.downloadProgress.load();
                ImGui::TextColored(ImVec4(0.20f, 0.74f, 0.44f, 1.00f), "%s %s", GetSpinnerChar(), g_LauncherUI.statusText.c_str());

                if (prog > 0.0f)
                {
                    char overlayBuf[32];
                    snprintf(overlayBuf, sizeof(overlayBuf), "Downloading Assets... %.0f%%", prog * 100.0f);
                    ImGui::ProgressBar(prog, ImVec2(-1, 26), overlayBuf);
                }
                else
                {
                    ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(-1, 26), "Processing...");
                }
            }
            else
            {
                ImGui::TextDisabled("Status: %s", g_LauncherUI.statusText.c_str());
                ImGui::Spacing();

                // Prominent Green Launch Button
                if (ImGui::Button("PLAY MINECRAFT", ImVec2(-1, 48)))
                {
                    if (g_LauncherUI.isOnlineMode)
                    {
                        g_LauncherUI.statusText = "Online mode not configured yet.";
                    }
                    else
                    {
                        g_LauncherUI.isBusy = true;
                        g_LauncherUI.statusText = "Syncing assets & launching...";
                        g_LauncherUI.downloadProgress = 0.0f;

                        // Background Launch Thread
                        std::thread([]() {
                            PlayerProfile profile = MinecraftLauncher::CreateOfflineProfile(g_LauncherUI.username);
                            std::string chosenVer = !g_LauncherUI.versions.empty() ? 
                                                    g_LauncherUI.versions[g_LauncherUI.selected_version].id : "1.20.4";

                            bool launched = MinecraftLauncher::LaunchGame(profile, chosenVer, g_LauncherUI.ramAllocation, g_LauncherUI.downloadProgress);
                            
                            if (launched)
                            {
                                g_LauncherUI.statusText = "Game Running!";
                                g_LauncherUI.versions = MinecraftLauncher::FetchVersions();
                            }
                            else
                            {
                                g_LauncherUI.statusText = "Launch Failed! Check console output.";
                            }

                            g_LauncherUI.isBusy = false;
                        }).detach();
                    }
                }
            }
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        // TAB 2: SETTINGS & SYSTEM INFO
        if (ImGui::BeginTabItem("Settings"))
        {
            ImGui::Spacing();
            char* appDataEnv = std::getenv("APPDATA");
            std::string gameDir = appDataEnv ? std::string(appDataEnv) + "\\.minecraft" : "N/A";

            ImGui::BeginChild("SystemCard", ImVec2(0, 0), true);
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "SYSTEM & PATHS");
            ImGui::Separator();

            ImGui::Text("Minecraft Root Path:");
            ImGui::TextDisabled("%s", gameDir.c_str());

            ImGui::Spacing();
            ImGui::Text("CPU Threads Available: %u", std::thread::hardware_concurrency());
            ImGui::Text("Parallel Download Pool: 8 Workers");
            ImGui::Text("Graphics Backend: Vulkan + ImGui");
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}