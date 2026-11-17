#include "ui/settings_page.hpp"
#include "ui/header.hpp"
#include "ui/theme.hpp"
#include "actions.hpp"
#include "config.hpp"
#include "engine/engine.h"
#include "platform_utils.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstring>

void SettingsPage::DrawSidebar(float height)
{
    constexpr float sidebarWidth = 236.0f;

    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgHeader);
    ImGui::BeginChild("##SettingsSidebar", ImVec2(sidebarWidth, height), false, ImGuiWindowFlags_NoScrollbar);

    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 wPos = ImGui::GetWindowPos();
    draw->AddLine(ImVec2(wPos.x + sidebarWidth - 1.0f, wPos.y), ImVec2(wPos.x + sidebarWidth - 1.0f, wPos.y + height), UITheme::BorderSubtle, 1.0f);

    ImGui::SetCursorPos(ImVec2(20.0f, 20.0f));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (ImGui::Button("<  BACK TO PLAY", ImVec2(196.0f, 40.0f)))
    {
        g_Launcher.currentPage = 0;
    }

    ImGui::Dummy(ImVec2(0.0f, 16.0f));
    ImGui::SetCursorPosX(22.0f);
    ImGui::TextColored(ImVec4(0.50f, 0.56f, 0.66f, 1.0f), "PREFERENCES");
    if (g_FontBold) ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    struct TabDef
    {
        const char *label;
        int idx;
    };
    TabDef tabs[] = {
        {"General & Paths", 0},
        {"Java & Performance", 1},
        {"Account & Profile", 2},
        {"Launcher Log", 3}
    };

    for (const auto &tab : tabs)
    {
        bool active = (g_Launcher.settingsPage == tab.idx);
        ImGui::SetCursorPosX(14.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? UITheme::EmeraldSoft : IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::BgSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? UITheme::Emerald : UITheme::TextPrimary);
        if (ImGui::Button(tab.label, ImVec2(208.0f, 42.0f)))
        {
            g_Launcher.settingsPage = tab.idx;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void SettingsPage::DrawGeneral()
{
    if (g_FontTitle) ImGui::PushFont(g_FontTitle);
    ImGui::Text("General & Paths");
    if (g_FontTitle) ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Configure Minecraft directory, launcher behavior, and mod isolation.");
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // Behavior card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgSurface);
    ImGui::BeginChild("##GeneralCard1", ImVec2(0.0f, 184.0f), true);
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::Text("Launcher Behavior");
    if (g_FontBold) ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (ImGui::Checkbox("Close launcher automatically after Minecraft starts", &g_Launcher.closeAfterLaunch))
        SaveConfig();
    if (ImGui::Checkbox("Show Minecraft Snapshot versions in installation list", &g_Launcher.showSnapshots))
    {
        SaveConfig();
        RefreshVersionsAsync();
    }
    if (ImGui::Checkbox("Capture Minecraft output into Launcher Console Log", &g_Launcher.captureGameLogs))
        SaveConfig();
    if (ImGui::Checkbox("Isolate Forge / modded profiles into separate instances/ folders", &g_Launcher.isolateModdedDirs))
    {
        g_Launcher.modsNeedRescan = true;
        SaveConfig();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // Directory card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgSurface);
    ImGui::BeginChild("##GeneralCard2", ImVec2(0.0f, 140.0f), true);
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::Text("Minecraft Root Directory (.minecraft)");
    if (g_FontBold) ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Versions, libraries, assets, and default mods/ are stored here.");
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(240.0f, availW - 220.0f));
    ImGui::InputText("##MinecraftDirectory", g_Launcher.minecraftDirectory, IM_ARRAYSIZE(g_Launcher.minecraftDirectory));

    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button("Browse...", ImVec2(100.0f, 34.0f)))
    {
        std::string chosen = PlatformUtils::OpenFolderDialog(L"Select .minecraft Directory", g_Launcher.GetGameDir());
        if (!chosen.empty())
        {
            std::strncpy(g_Launcher.minecraftDirectory, chosen.c_str(), sizeof(g_Launcher.minecraftDirectory) - 1);
            g_Launcher.minecraftDirectory[sizeof(g_Launcher.minecraftDirectory) - 1] = '\0';
            g_Launcher.modsNeedRescan = true;
            SaveConfig();
            RefreshVersionsAsync();
        }
    }

    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button("Open", ImVec2(90.0f, 34.0f)))
    {
        PlatformUtils::OpenFolderInExplorer(g_Launcher.GetGameDir());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, UITheme::Emerald);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::EmeraldHover);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(8, 20, 12, 255));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (ImGui::Button("SAVE GENERAL SETTINGS", ImVec2(220.0f, 42.0f)))
    {
        g_Launcher.modsNeedRescan = true;
        SaveConfig();
        RefreshVersionsAsync();
        g_Launcher.SetStatus("General settings saved!");
    }
    if (g_FontBold) ImGui::PopFont();
    ImGui::PopStyleColor(3);
}

void SettingsPage::DrawJavaAndPerformance()
{
    if (g_FontTitle) ImGui::PushFont(g_FontTitle);
    ImGui::Text("Java & Performance");
    if (g_FontTitle) ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Optimize launcher GPU/CPU usage and configure Java memory & runtime.");
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // Memory & Java card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgSurface);
    ImGui::BeginChild("##PerfCardJava", ImVec2(0.0f, 268.0f), true);

    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::Text("Memory Allocation (RAM)");
    if (g_FontBold) ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f),
                       "Detected System RAM: %d GB (Recommended 4-8 GB for Forge modpacks)",
                       g_Launcher.systemMemoryGB);
    int maxSliderRam = std::max(4, std::min(32, g_Launcher.systemMemoryGB));
    ImGui::SetNextItemWidth(std::min(520.0f, ImGui::GetContentRegionAvail().x));
    if (ImGui::SliderInt("##RamAllocation", &g_Launcher.ramAllocation, 1, maxSliderRam, "%d GB RAM"))
    {
        SaveConfig();
    }

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::Text("Java Runtime Executable (Optional Override)");
    if (g_FontBold) ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f),
                       "Leave blank to auto-select best Java (Java 8 for MC <=1.16, Java 17/21+ for modern Forge).");

    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(220.0f, availW - 220.0f));
    ImGui::InputTextWithHint("##CustomJavaPath", "Auto-detect best Java runtime...", g_Launcher.customJavaPath, IM_ARRAYSIZE(g_Launcher.customJavaPath));
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button("Browse java.exe", ImVec2(126.0f, 34.0f)))
    {
        auto files = PlatformUtils::OpenFileDialog(L"Select java.exe", L"Java Executable (java.exe)", L"java.exe", false);
        if (!files.empty())
        {
            std::strncpy(g_Launcher.customJavaPath, files[0].c_str(), sizeof(g_Launcher.customJavaPath) - 1);
            g_Launcher.customJavaPath[sizeof(g_Launcher.customJavaPath) - 1] = '\0';
            SaveConfig();
        }
    }
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button("Auto", ImVec2(66.0f, 34.0f)))
    {
        g_Launcher.customJavaPath[0] = '\0';
        SaveConfig();
    }

    if (!g_Launcher.javaRuntimes.empty())
    {
        ImGui::SetNextItemWidth(std::min(560.0f, availW - 110.0f));
        if (ImGui::BeginCombo("##DetectedJavas", "Quick-pick detected Java installation..."))
        {
            for (const auto &rt : g_Launcher.javaRuntimes)
            {
                if (ImGui::Selectable(rt.label.c_str()))
                {
                    std::strncpy(g_Launcher.customJavaPath, rt.path.c_str(), sizeof(g_Launcher.customJavaPath) - 1);
                    g_Launcher.customJavaPath[sizeof(g_Launcher.customJavaPath) - 1] = '\0';
                    SaveConfig();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (ImGui::Button("Rescan", ImVec2(90.0f, 32.0f)))
        {
            g_Launcher.javaRuntimes = PlatformUtils::ScanJavaRuntimes();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::SetNextItemWidth(std::min(660.0f, availW));
    ImGui::InputTextWithHint("##CustomJvmArgs", "Extra JVM flags (e.g. -XX:+UseG1GC -XX:+UnlockExperimentalVMOptions)", g_Launcher.customJvmArgs, IM_ARRAYSIZE(g_Launcher.customJvmArgs));

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // Launcher UI Performance Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgSurface);
    ImGui::BeginChild("##PerfCardEngine", ImVec2(0.0f, 126.0f), true);
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::Text("Launcher Rendering Performance");
    if (g_FontBold) ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (ImGui::Checkbox("Enable VSync (limits GPU frame rate to display refresh)", &g_Launcher.vsyncEnabled))
    {
        OLIA_ENGINE::SetVSync(g_Launcher.vsyncEnabled);
        SaveConfig();
    }
    if (ImGui::Checkbox("Idle Power Saving (reduces CPU/GPU usage when idle or unfocused)", &g_Launcher.powerSavingMode))
    {
        OLIA_ENGINE::SetPowerSaving(g_Launcher.powerSavingMode);
        SaveConfig();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, UITheme::Emerald);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::EmeraldHover);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(8, 20, 12, 255));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (ImGui::Button("SAVE PERFORMANCE SETTINGS", ImVec2(250.0f, 42.0f)))
    {
        SaveConfig();
        g_Launcher.SetStatus("Java & Performance settings saved!");
    }
    if (g_FontBold) ImGui::PopFont();
    ImGui::PopStyleColor(3);
}

void SettingsPage::DrawProfile()
{
    if (g_FontTitle) ImGui::PushFont(g_FontTitle);
    ImGui::Text("Account & Profile");
    if (g_FontTitle) ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Configure your offline player name or authenticate with Microsoft.");
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgSurface);
    ImGui::BeginChild("##ProfileCard", ImVec2(0.0f, 290.0f), true);

    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::Text("Player Username");
    if (g_FontBold) ImGui::PopFont();
    ImGui::SetNextItemWidth(380.0f);
    ImGui::InputText("##ProfileName", g_Launcher.profileName, IM_ARRAYSIZE(g_Launcher.profileName));

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::Text("Authentication Mode");
    if (g_FontBold) ImGui::PopFont();

    const char *accountTypes[] = {"Nolan Account (Offline)", "Microsoft Account (Online)"};
    int currentIdx = g_Launcher.isOnlineMode ? 1 : 0;

    ImGui::SetNextItemWidth(380.0f);
    if (ImGui::Combo("##AccountTypeCombo", &currentIdx, accountTypes, IM_ARRAYSIZE(accountTypes)))
    {
        g_Launcher.isOnlineMode = (currentIdx == 1);
        std::strncpy(g_Launcher.accountType, accountTypes[currentIdx], sizeof(g_Launcher.accountType) - 1);
        g_Launcher.accountType[sizeof(g_Launcher.accountType) - 1] = '\0';
        SaveConfig();
    }

    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    if (g_Launcher.isOnlineMode)
    {
        if (g_Launcher.isAuthenticated)
        {
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.45f, 1.0f), "Authenticated as Microsoft User: %s", g_Launcher.profile.username.c_str());
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            if (ImGui::Button("Sign Out", ImVec2(120.0f, 36.0f)))
            {
                g_Launcher.isAuthenticated = false;
                g_Launcher.profile = PlayerProfile{};
                SaveConfig();
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, UITheme::BlueAccent);
            if (ImGui::Button("LOG IN WITH MICROSOFT", ImVec2(240.0f, 40.0f)))
            {
                StartMSALoginAsync();
            }
            ImGui::PopStyleColor();
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "Offline mode generates a standard Minecraft MD5 UUID for local & offline servers.");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, UITheme::Emerald);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::EmeraldHover);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(8, 20, 12, 255));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (ImGui::Button("SAVE PROFILE", ImVec2(190.0f, 42.0f)))
    {
        std::strncpy(g_Launcher.username, g_Launcher.profileName, sizeof(g_Launcher.username) - 1);
        g_Launcher.username[sizeof(g_Launcher.username) - 1] = '\0';
        SaveConfig();
        g_Launcher.SetStatus("Profile saved!");
    }
    if (g_FontBold) ImGui::PopFont();
    ImGui::PopStyleColor(3);
}

void SettingsPage::DrawLogs()
{
    if (g_FontTitle) ImGui::PushFont(g_FontTitle);
    ImGui::Text("Launcher & Game Console Log");
    if (g_FontTitle) ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Live diagnostic output from downloads, Forge installation, and Minecraft.");
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    auto logs = g_Launcher.GetLogsSnapshot();

    if (ImGui::Button("Copy Log", ImVec2(110.0f, 34.0f)))
    {
        std::string all;
        for (const auto &l : logs)
        {
            all += l;
            all += "\n";
        }
        ImGui::SetClipboardText(all.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(90.0f, 34.0f)))
    {
        g_Launcher.ClearLogs();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgInput);
    ImGui::BeginChild("##SettingsLogBox", ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 10.0f), true);
    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    for (const auto &line : logs)
    {
        ImVec4 col(0.82f, 0.86f, 0.92f, 1.0f);
        if (line.find("ERROR") != std::string::npos || line.find("failed") != std::string::npos)
            col = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
        else if (line.find("[Forge]") != std::string::npos)
            col = ImVec4(0.95f, 0.64f, 0.25f, 1.0f);
        ImGui::TextColored(col, "%s", line.c_str());
    }
    if (g_FontSmall) ImGui::PopFont();
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void SettingsPage::Draw()
{
    ImGuiIO &io = ImGui::GetIO();
    float consoleHeight = g_Launcher.showConsoleDrawer ? 210.0f : 0.0f;
    float pageY = Header::Height;
    float pageH = std::max(200.0f, io.DisplaySize.y - Header::Height - consoleHeight);

    ImGui::SetNextWindowPos(ImVec2(0.0f, pageY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, pageH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##SettingsPage", nullptr, flags);

    DrawSidebar(pageH);

    constexpr float sidebarWidth = 236.0f;
    ImGui::SetCursorPos(ImVec2(sidebarWidth + 32.0f, 24.0f));
    ImGui::BeginChild("##SettingsContent", ImVec2(io.DisplaySize.x - sidebarWidth - 64.0f, pageH - 40.0f), false, ImGuiWindowFlags_NoBackground);

    switch (g_Launcher.settingsPage)
    {
    case 0:
        DrawGeneral();
        break;
    case 1:
        DrawJavaAndPerformance();
        break;
    case 2:
        DrawProfile();
        break;
    default:
        DrawLogs();
        break;
    }

    ImGui::EndChild();
    ImGui::End();
}
