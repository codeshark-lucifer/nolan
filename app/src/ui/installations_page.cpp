#include "ui/installations_page.hpp"
#include "ui/header.hpp"
#include "ui/theme.hpp"
#include "actions.hpp"
#include "config.hpp"
#include "platform_utils.hpp"
#include "state.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    std::string LowerAscii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }
} // namespace

void InstallationsPage::Draw()
{
    ImGuiIO &io = ImGui::GetIO();
    float consoleHeight = g_Launcher.showConsoleDrawer ? 210.0f : 0.0f;
    float pageY = Header::Height;
    float pageH = std::max(200.0f, io.DisplaySize.y - Header::Height - consoleHeight);

    ImGui::SetNextWindowPos(ImVec2(0.0f, pageY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, pageH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##InstallationsPage", nullptr, flags);

    constexpr float padX = 28.0f;
    float contentW = io.DisplaySize.x - padX * 2.0f;

    static char searchBuf[128] = "";
    static int filterTab = 0; // 0 = All, 1 = Installed, 2 = Forge / Modded, 3 = Releases

    // Title Row
    ImGui::SetCursorPos(ImVec2(padX, 20.0f));
    ImGui::BeginGroup();
    if (g_FontTitle) ImGui::PushFont(g_FontTitle);
    ImGui::TextColored(ImVec4(0.96f, 0.97f, 0.99f, 1.0f), "INSTALLATIONS");
    if (g_FontTitle) ImGui::PopFont();
    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Manage Minecraft releases, Forge modded profiles, and local installations.");
    if (g_FontSmall) ImGui::PopFont();
    ImGui::EndGroup();

    // Right Action Buttons
    float btnH = 38.0f;
    float forgeBtnW = 166.0f;
    float jarBtnW = 178.0f;
    ImGui::SetCursorPos(ImVec2(padX + contentW - forgeBtnW - jarBtnW - 10.0f, 22.0f));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (UITheme::DrawIconButton(
            "##InstImportForgeJar", "IMPORT FORGE .JAR",
            UITheme::IconType::DropJar, ImVec2(jarBtnW, btnH),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::TextPrimary, 8.0f))
    {
        auto files = PlatformUtils::OpenFileDialog(
            L"Select Forge Installer .jar",
            L"Forge Installer JAR (*.jar)",
            L"*.jar",
            false);
        if (!files.empty())
        {
            std::strncpy(g_Launcher.localForgeJarPath, files[0].c_str(), sizeof(g_Launcher.localForgeJarPath) - 1);
            g_Launcher.localForgeJarPath[sizeof(g_Launcher.localForgeJarPath) - 1] = '\0';
            g_Launcher.localForgeInspection = ForgeInstaller::InspectForgeInstallerJar(files[0]);
            g_Launcher.forgeModalTab = 1;
            g_Launcher.showForgeInstallerModal = true;
        }
    }

    ImGui::SameLine(0.0f, 10.0f);
    if (UITheme::DrawIconButton(
            "##InstGetForge", "INSTALL FORGE",
            UITheme::IconType::Anvil, ImVec2(forgeBtnW, btnH),
            UITheme::ForgeAmber, UITheme::ForgeAmberHover, UITheme::ForgeAmberActive,
            IM_COL32(18, 12, 6, 255), 8.0f))
    {
        g_Launcher.forgeModalTab = 0;
        g_Launcher.showForgeInstallerModal = true;
        RefreshForgePromotionsAsync();
    }
    if (g_FontBold) ImGui::PopFont();

    // Filter & Search Toolbar
    ImGui::SetCursorPos(ImVec2(padX, 82.0f));
    ImGui::SetNextItemWidth(std::min(340.0f, contentW * 0.34f));
    ImGui::InputTextWithHint("##InstSearch", "Search versions (e.g. 1.20.1, forge)...", searchBuf, sizeof(searchBuf));

    const char *filterLabels[] = {"All", "Installed", "Forge / Modded", "Releases"};
    for (int f = 0; f < 4; ++f)
    {
        ImGui::SameLine(0.0f, 8.0f);
        bool active = (filterTab == f);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? UITheme::EmeraldSoft : UITheme::BgSurfaceElevated);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::BgSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? UITheme::Emerald : UITheme::TextSecondary);
        if (ImGui::Button(filterLabels[f], ImVec2(0.0f, 34.0f)))
        {
            filterTab = f;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0.0f, 14.0f);
    bool snaps = g_Launcher.showSnapshots;
    if (ImGui::Checkbox("Show Snapshots", &snaps))
    {
        g_Launcher.showSnapshots = snaps;
        SaveConfig();
        RefreshVersionsAsync();
    }

    // Build filtered list snapshot
    std::vector<VersionEntry> filtered;
    {
        std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
        std::string query = LowerAscii(searchBuf);
        filtered.reserve(g_Launcher.versions.size());
        for (const auto &v : g_Launcher.versions)
        {
            if (filterTab == 1 && !v.isDownloaded)
                continue;
            if (filterTab == 2 && !v.isModded)
                continue;
            if (filterTab == 3 && (v.isModded || v.type != "release"))
                continue;
            if (!query.empty() && LowerAscii(v.id).find(query) == std::string::npos &&
                LowerAscii(v.loaderName).find(query) == std::string::npos)
                continue;
            filtered.push_back(v);
        }
    }

    // List Card Container
    float listY = 128.0f;
    float listH = std::max(120.0f, pageH - listY - 22.0f);
    ImGui::SetCursorPos(ImVec2(padX, listY));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgSurface);
    ImGui::BeginChild("##InstallationsList", ImVec2(contentW, listH), true);

    if (filtered.empty())
    {
        ImGui::SetCursorPos(ImVec2(24.0f, 30.0f));
        ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "No matching versions found.");
    }
    else
    {
        const bool busy = g_Launcher.isBusy.load();
        const std::string baseDir = g_Launcher.GetGameDir();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filtered.size()), 72.0f);
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const auto &ver = filtered[static_cast<size_t>(row)];
                ImGui::PushID(row);

                ImVec2 rowMin = ImGui::GetCursorScreenPos();
                float rowW = ImGui::GetContentRegionAvail().x;
                float rowH = 64.0f;
                ImVec2 rowMax(rowMin.x + rowW, rowMin.y + rowH);
                ImDrawList *draw = ImGui::GetWindowDrawList();

                draw->AddRectFilled(rowMin, rowMax, UITheme::BgSurfaceElevated, 10.0f);
                draw->AddRect(rowMin, rowMax, ver.isModded ? IM_COL32(238, 142, 45, 70) : UITheme::BorderSubtle, 10.0f, 0, 1.0f);

                UITheme::DrawMinecraftBlockIcon(
                    draw,
                    ImVec2(rowMin.x + 14.0f, rowMin.y + 12.0f),
                    40.0f,
                    ver.isModded,
                    ver.type == "snapshot");

                // Title & badges
                ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 68.0f, rowMin.y + 10.0f));
                ImGui::BeginGroup();
                if (g_FontBold) ImGui::PushFont(g_FontBold);
                ImGui::TextUnformatted(ver.id.c_str());
                if (g_FontBold) ImGui::PopFont();

                if (ver.isModded)
                {
                    ImGui::SameLine(0.0f, 10.0f);
                    UITheme::DrawBadge(ver.loaderName.empty() ? "FORGE" : ver.loaderName.c_str(), UITheme::ForgeAmberSoft, UITheme::ForgeAmber, 20.0f, 8.0f);
                }
                if (ver.isDownloaded)
                {
                    ImGui::SameLine(0.0f, 6.0f);
                    UITheme::DrawBadge("INSTALLED", UITheme::EmeraldSoft, UITheme::Emerald, 20.0f, 8.0f);
                }

                if (g_FontSmall) ImGui::PushFont(g_FontSmall);
                if (ver.isModded && !ver.inheritsFrom.empty())
                {
                    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Modded profile based on Minecraft %s", ver.inheritsFrom.c_str());
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "%s",
                                       ver.isDownloaded ? "Ready to launch locally" : "Available for download from Mojang");
                }
                if (g_FontSmall) ImGui::PopFont();
                ImGui::EndGroup();

                // Action Buttons on Right
                float actionsW = ver.isDownloaded ? (ver.isModded ? 344.0f : 260.0f) : 172.0f;
                ImGui::SetCursorScreenPos(ImVec2(rowMax.x - actionsW - 14.0f, rowMin.y + 14.0f));
                ImGui::BeginDisabled(busy);

                if (ver.isDownloaded)
                {
                    if (UITheme::DrawIconButton(
                            "##PlayVer", "PLAY",
                            UITheme::IconType::Play, ImVec2(96.0f, 36.0f),
                            UITheme::Emerald, UITheme::EmeraldHover, UITheme::EmeraldActive,
                            IM_COL32(8, 20, 12, 255), 8.0f))
                    {
                        g_Launcher.SelectVersionById(ver.id);
                        SaveConfig();
                        LaunchMinecraftAsync(ver.id);
                    }

                    if (ver.isModded)
                    {
                        ImGui::SameLine(0.0f, 8.0f);
                        if (UITheme::DrawIconButton(
                                "##ModsVer", "MODS",
                                UITheme::IconType::Cube, ImVec2(80.0f, 36.0f),
                                UITheme::ForgeAmberSoft, IM_COL32(238, 142, 45, 70), UITheme::ForgeAmber,
                                UITheme::ForgeAmber, 8.0f))
                        {
                            g_Launcher.SelectVersionById(ver.id);
                            g_Launcher.currentPage = 2;
                            g_Launcher.modsNeedRescan = true;
                        }
                    }

                    ImGui::SameLine(0.0f, 8.0f);
                    if (UITheme::DrawIconButton(
                            "##OpenVer", "",
                            UITheme::IconType::Folder, ImVec2(36.0f, 36.0f),
                            UITheme::BgSurface, UITheme::BgSurfaceHover, UITheme::BgInput,
                            UITheme::TextPrimary, 8.0f))
                    {
                        PlatformUtils::OpenFolderInExplorer((fs::path(baseDir) / "versions" / ver.id).string());
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Open version folder");

                    ImGui::SameLine(0.0f, 8.0f);
                    if (UITheme::DrawIconButton(
                            "##RemoveVer", "REMOVE",
                            UITheme::IconType::Trash, ImVec2(104.0f, 36.0f),
                            UITheme::DangerSoft, IM_COL32(239, 68, 68, 80), UITheme::DangerRed,
                            UITheme::DangerRedHover, 8.0f))
                    {
                        g_Launcher.versionToUninstall = ver.id;
                        g_Launcher.showUninstallDialog = true;
                    }
                }
                else
                {
                    if (UITheme::DrawIconButton(
                            "##DownloadVer", "INSTALL & PLAY",
                            UITheme::IconType::Play, ImVec2(172.0f, 36.0f),
                            UITheme::BgSurface, UITheme::EmeraldSoft, UITheme::Emerald,
                            UITheme::Emerald, 8.0f))
                    {
                        g_Launcher.SelectVersionById(ver.id);
                        SaveConfig();
                        LaunchMinecraftAsync(ver.id);
                    }
                }

                ImGui::EndDisabled();
                ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y + 8.0f));
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
                ImGui::PopID();
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();

    if (g_Launcher.showUninstallDialog)
    {
        ImGui::OpenPopup("Uninstall Confirmation");
    }

    if (ImGui::BeginPopupModal("Uninstall Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (g_FontBold) ImGui::PushFont(g_FontBold);
        ImGui::Text("Uninstall version %s?", g_Launcher.versionToUninstall.c_str());
        if (g_FontBold) ImGui::PopFont();
        ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "This removes local client files from versions/%s.", g_Launcher.versionToUninstall.c_str());
        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        ImGui::PushStyleColor(ImGuiCol_Button, UITheme::DangerRed);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::DangerRedHover);
        if (ImGui::Button("Yes, Uninstall", ImVec2(130.0f, 36.0f)))
        {
            UninstallVersionAsync(g_Launcher.versionToUninstall);
            g_Launcher.showUninstallDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 36.0f)))
        {
            g_Launcher.showUninstallDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
