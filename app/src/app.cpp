#include "app.hpp"
#include "actions.hpp"
#include "config.hpp"
#include "engine/engine.h"
#include "platform_utils.hpp"
#include "state.hpp"

#include "ui/forge_modal.hpp"
#include "ui/header.hpp"
#include "ui/installations_page.hpp"
#include "ui/mods_page.hpp"
#include "ui/play_page.hpp"
#include "ui/settings_page.hpp"
#include "ui/theme.hpp"

#include <cstdlib>
#include <cstring>
#include <vector>

void setup()
{
    ImGui::SetCurrentContext(OLIA_ENGINE::GetImguiContext());
    UITheme::ApplyModernStyle();

    ImGuiIO &io = ImGui::GetIO();
    g_FontRegular = io.Fonts->AddFontFromFileTTF("assets/fonts/default-regular.ttf", 17.0f);
    g_FontSmall   = io.Fonts->AddFontFromFileTTF("assets/fonts/default-regular.ttf", 14.0f);
    g_FontBold    = io.Fonts->AddFontFromFileTTF("assets/fonts/deafult-bold.ttf", 17.0f);
    g_FontTitle   = io.Fonts->AddFontFromFileTTF("assets/fonts/deafult-bold.ttf", 22.0f);
    g_FontHero    = io.Fonts->AddFontFromFileTTF("assets/fonts/deafult-bold.ttf", 32.0f);

    if (!g_FontRegular) g_FontRegular = io.FontDefault;
    if (!g_FontSmall)   g_FontSmall   = g_FontRegular;
    if (!g_FontBold)    g_FontBold    = g_FontRegular;
    if (!g_FontTitle)   g_FontTitle   = g_FontBold;
    if (!g_FontHero)    g_FontHero    = g_FontBold;

    // Upload crisp launcher artwork textures once
    {
        auto iconTex = OLIA_ENGINE::LoadUITextureFromFile("assets/icon.png");
        g_Textures.appIcon = static_cast<ImTextureID>(iconTex.id);

        auto wallTex = OLIA_ENGINE::LoadUITextureFromFile("assets/textures/wall.jpg");
        g_Textures.heroWall = static_cast<ImTextureID>(wallTex.id);
        g_Textures.heroWallWidth = wallTex.width;
        g_Textures.heroWallHeight = wallTex.height;

        auto profTex = OLIA_ENGINE::LoadUITextureFromFile("assets/textures/profile.png");
        g_Textures.profileAvatar = static_cast<ImTextureID>(profTex.id);
    }

    if (const char *appData = std::getenv("APPDATA"))
    {
        std::string defDir = std::string(appData) + "\\.minecraft";
        std::strncpy(g_Launcher.minecraftDirectory, defDir.c_str(), sizeof(g_Launcher.minecraftDirectory) - 1);
        g_Launcher.minecraftDirectory[sizeof(g_Launcher.minecraftDirectory) - 1] = '\0';
    }

    g_Launcher.systemMemoryGB = PlatformUtils::GetSystemMemoryGB();
    g_Launcher.javaRuntimes = PlatformUtils::ScanJavaRuntimes();

    LoadConfig();
    RefreshModsCache(true);
    RefreshVersionsAsync();
}

static void DrawConsoleDrawer()
{
    if (!g_Launcher.showConsoleDrawer)
        return;

    ImGuiIO &io = ImGui::GetIO();
    constexpr float drawerH = 210.0f;

    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - drawerH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, drawerH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, UITheme::BgHeader);
    ImGui::Begin("##ConsoleDrawer", nullptr, flags);

    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.45f, 1.0f), ">_ LIVE CONSOLE");
    if (g_FontBold) ImGui::PopFont();

    auto logs = g_Launcher.GetLogsSnapshot();

    ImGui::SameLine(io.DisplaySize.x - 270.0f);
    if (ImGui::Button("Copy", ImVec2(74.0f, 26.0f)))
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
    if (ImGui::Button("Clear", ImVec2(74.0f, 26.0f)))
    {
        g_Launcher.ClearLogs();
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide", ImVec2(74.0f, 26.0f)))
    {
        g_Launcher.showConsoleDrawer = false;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgInput);
    ImGui::BeginChild("##DrawerScroll", ImVec2(0.0f, 0.0f), true);
    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    for (const auto &line : logs)
    {
        ImVec4 col(0.80f, 0.85f, 0.92f, 1.0f);
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

    ImGui::End();
    ImGui::PopStyleColor();
}

static void DrawDropOverlay()
{
    if (!OLIA_ENGINE::IsFileDropHovering())
        return;

    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *fg = ImGui::GetForegroundDrawList();
    fg->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(8, 12, 18, 200));

    ImVec2 padMin(36.0f, 36.0f);
    ImVec2 padMax(io.DisplaySize.x - 36.0f, io.DisplaySize.y - 36.0f);
    fg->AddRect(padMin, padMax, UITheme::ForgeAmber, 18.0f, 0, 3.0f);

    ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f - 20.0f);
    UITheme::DrawVectorIcon(fg, UITheme::IconType::DropJar, c, 54.0f, UITheme::ForgeAmber);

    const char *msg = "Drop Forge Mod (.jar) or Forge Installer (.jar) here";
    if (g_FontTitle) ImGui::PushFont(g_FontTitle);
    ImVec2 ts = ImGui::CalcTextSize(msg);
    fg->AddText(ImVec2(c.x - ts.x * 0.5f, c.y + 42.0f), UITheme::TextPrimary, msg);
    if (g_FontTitle) ImGui::PopFont();
}

void draw()
{
    // Process drag-and-dropped files from SDL3
    uint32_t dropCount = OLIA_ENGINE::PollDroppedFileCount();
    if (dropCount > 0)
    {
        std::vector<std::string> dropped;
        dropped.reserve(dropCount);
        for (uint32_t i = 0; i < dropCount; ++i)
            dropped.emplace_back(OLIA_ENGINE::GetDroppedFileAt(i));
        OLIA_ENGINE::ClearDroppedFiles();
        HandleDroppedFiles(dropped);
    }

    // Keep frames smooth while background download/installation/modals run
    if (g_Launcher.isBusy.load() || g_Launcher.showAuthModal || g_Launcher.showForgeInstallerModal)
    {
        OLIA_ENGINE::RequestActiveFrame();
    }

    Header::Draw();

    switch (g_Launcher.currentPage)
    {
    case 0:
    {
        PlayPage page;
        page.Draw();
        break;
    }
    case 1:
    {
        InstallationsPage page;
        page.Draw();
        break;
    }
    case 2:
    {
        ModsPage page;
        page.Draw();
        break;
    }
    case 3:
    {
        SettingsPage page;
        page.Draw();
        break;
    }
    }

    DrawConsoleDrawer();
    ForgeModal::Draw();
    DrawDropOverlay();

    // Dropped Forge Installer Confirmation
    if (g_Launcher.showDropInstallPrompt)
    {
        ImGui::OpenPopup("Forge Installer Detected");
    }
    if (ImGui::BeginPopupModal("Forge Installer Detected", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (g_FontBold) ImGui::PushFont(g_FontBold);
        ImGui::TextColored(ImVec4(0.95f, 0.62f, 0.22f, 1.0f), "Detected a Forge Installer .jar!");
        if (g_FontBold) ImGui::PopFont();
        ImGui::Text("File: %s", g_Launcher.pendingDroppedInstallerJar.c_str());
        if (g_Launcher.pendingDroppedInspection.valid)
        {
            ImGui::Text("Target: %s (MC %s)",
                        g_Launcher.pendingDroppedInspection.targetVersionId.c_str(),
                        g_Launcher.pendingDroppedInspection.mcVersion.c_str());
        }
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        if (ImGui::Button("Install Forge Now", ImVec2(170.0f, 36.0f)))
        {
            std::string jarPath = g_Launcher.pendingDroppedInstallerJar;
            g_Launcher.showDropInstallPrompt = false;
            ImGui::CloseCurrentPopup();
            InstallForgeFromJarAsync(jarPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy to mods/ instead", ImVec2(170.0f, 36.0f)))
        {
            ImportModJarsAndNotify({g_Launcher.pendingDroppedInstallerJar});
            g_Launcher.showDropInstallPrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 36.0f)))
        {
            g_Launcher.showDropInstallPrompt = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Modal check for Microsoft Device Authentication
    if (g_Launcher.showAuthModal)
    {
        ImGui::OpenPopup("Microsoft Device Login");
    }

    if (ImGui::BeginPopupModal("Microsoft Device Login", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("1. Open the following link in your browser:");
        ImGui::TextColored(ImVec4(0.35f, 0.72f, 1.0f, 1.0f), "%s", g_Launcher.msaCode.verificationUri.c_str());

        ImGui::Spacing();
        ImGui::Text("2. Enter this device code:");
        if (g_FontTitle) ImGui::PushFont(g_FontTitle);
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "%s", g_Launcher.msaCode.userCode.c_str());
        if (g_FontTitle) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Copy Code & Open Link", ImVec2(210.0f, 38.0f)))
        {
            ImGui::SetClipboardText(g_Launcher.msaCode.userCode.c_str());
            PlatformUtils::OpenUrlInBrowser(g_Launcher.msaCode.verificationUri);
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 38.0f)))
        {
            g_Launcher.cancelAuth = true;
            g_Launcher.showAuthModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void loop()
{
    draw();
}
