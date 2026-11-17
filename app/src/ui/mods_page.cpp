#include "ui/mods_page.hpp"
#include "ui/header.hpp"
#include "ui/theme.hpp"
#include "actions.hpp"
#include "engine/engine.h"
#include "platform_utils.hpp"
#include "state.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    std::string LowerStr(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    std::string FormatFileSize(uint64_t bytes)
    {
        char buf[32];
        if (bytes >= 1024ULL * 1024ULL)
            snprintf(buf, sizeof(buf), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        else if (bytes >= 1024ULL)
            snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
        else
            snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        return std::string(buf);
    }
} // namespace

void ModsPage::Draw()
{
    RefreshModsCache(false);

    ImGuiIO &io = ImGui::GetIO();
    float consoleHeight = g_Launcher.showConsoleDrawer ? 210.0f : 0.0f;
    float pageY = Header::Height;
    float pageH = std::max(200.0f, io.DisplaySize.y - Header::Height - consoleHeight);

    ImGui::SetNextWindowPos(ImVec2(0.0f, pageY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, pageH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##ModsPage", nullptr, flags);

    constexpr float padX = 28.0f;
    float contentW = io.DisplaySize.x - padX * 2.0f;

    static char searchBuf[128] = "";
    static int statusFilter = 0; // 0 = All, 1 = Enabled, 2 = Disabled

    // Title & Active Path
    std::string modsDir = g_Launcher.GetActiveModsDir();
    VersionEntry activeVer = g_Launcher.GetSelectedVersionCopy();

    ImGui::SetCursorPos(ImVec2(padX, 20.0f));
    ImGui::BeginGroup();
    if (g_FontTitle) ImGui::PushFont(g_FontTitle);
    ImGui::TextColored(ImVec4(0.96f, 0.97f, 0.99f, 1.0f), "FORGE MODS (.JAR)");
    if (g_FontTitle) ImGui::PopFont();
    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.74f, 1.0f), "Mods folder: %s", modsDir.c_str());
    if (g_FontSmall) ImGui::PopFont();
    ImGui::EndGroup();

    // Right Action Buttons
    float btnH = 38.0f;
    float addW = 162.0f;
    float folderW = 152.0f;
    float rescanW = 112.0f;
    ImGui::SetCursorPos(ImVec2(padX + contentW - addW - folderW - rescanW - 20.0f, 22.0f));

    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (UITheme::DrawIconButton(
            "##ModsRescanBtn", "RESCAN",
            UITheme::IconType::Refresh, ImVec2(rescanW, btnH),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::TextPrimary, 8.0f))
    {
        RefreshModsCache(true);
    }

    ImGui::SameLine(0.0f, 10.0f);
    if (UITheme::DrawIconButton(
            "##ModsOpenDirBtn", "OPEN FOLDER",
            UITheme::IconType::Folder, ImVec2(folderW, btnH),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::TextPrimary, 8.0f))
    {
        PlatformUtils::OpenFolderInExplorer(modsDir);
    }

    ImGui::SameLine(0.0f, 10.0f);
    if (UITheme::DrawIconButton(
            "##ModsAddJarBtn", "ADD MOD (.JAR)",
            UITheme::IconType::Plus, ImVec2(addW, btnH),
            UITheme::ForgeAmber, UITheme::ForgeAmberHover, UITheme::ForgeAmberActive,
            IM_COL32(18, 12, 6, 255), 8.0f))
    {
        auto files = PlatformUtils::OpenFileDialog(
            L"Select Forge Mod .jar Files",
            L"Minecraft Mod JAR (*.jar)",
            L"*.jar",
            true);
        if (!files.empty())
        {
            HandleDroppedFiles(files);
        }
    }
    if (g_FontBold) ImGui::PopFont();

    float curY = 82.0f;

    // Helpful banner if current version is Vanilla instead of Forge
    if (!activeVer.id.empty() && !activeVer.isModded)
    {
        ImVec2 bMin(padX, curY);
        ImVec2 bMax(padX + contentW, curY + 46.0f);
        ImVec2 sMin(ImGui::GetWindowPos().x + bMin.x, ImGui::GetWindowPos().y + bMin.y);
        ImVec2 sMax(ImGui::GetWindowPos().x + bMax.x, ImGui::GetWindowPos().y + bMax.y);
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(sMin, sMax, UITheme::ForgeAmberSoft, 10.0f);
        draw->AddRect(sMin, sMax, IM_COL32(238, 142, 45, 130), 10.0f, 0, 1.0f);

        ImGui::SetCursorPos(ImVec2(padX + 16.0f, curY + 13.0f));
        ImGui::TextColored(
            ImVec4(0.98f, 0.72f, 0.36f, 1.0f),
            "Selected version '%s' is Vanilla. Install or switch to Forge to load .jar mods in Minecraft!",
            activeVer.id.c_str());

        ImGui::SetCursorPos(ImVec2(padX + contentW - 156.0f, curY + 7.0f));
        if (g_FontBold) ImGui::PushFont(g_FontBold);
        if (UITheme::DrawIconButton(
                "##BannerInstallForge", "INSTALL FORGE",
                UITheme::IconType::Anvil, ImVec2(142.0f, 32.0f),
                UITheme::ForgeAmber, UITheme::ForgeAmberHover, UITheme::ForgeAmberActive,
                IM_COL32(18, 12, 6, 255), 8.0f))
        {
            g_Launcher.showForgeInstallerModal = true;
            RefreshForgePromotionsAsync();
        }
        if (g_FontBold) ImGui::PopFont();

        curY += 58.0f;
    }

    // Search & Filter Toolbar
    ImGui::SetCursorPos(ImVec2(padX, curY));
    ImGui::SetNextItemWidth(std::min(340.0f, contentW * 0.36f));
    ImGui::InputTextWithHint("##ModsSearch", "Search installed mods...", searchBuf, sizeof(searchBuf));

    const char *filterLabels[] = {"All", "Enabled", "Disabled"};
    for (int f = 0; f < 3; ++f)
    {
        ImGui::SameLine(0.0f, 8.0f);
        bool active = (statusFilter == f);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? UITheme::ForgeAmberSoft : UITheme::BgSurfaceElevated);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::BgSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? UITheme::ForgeAmber : UITheme::TextSecondary);
        if (ImGui::Button(filterLabels[f], ImVec2(0.0f, 34.0f)))
        {
            statusFilter = f;
        }
        ImGui::PopStyleColor(3);
    }

    int enabledCount = 0;
    uint64_t totalBytes = 0;
    for (const auto &m : g_Launcher.mods)
    {
        if (m.enabled)
            enabledCount++;
        totalBytes += m.fileSize;
    }

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::SetCursorPosY(curY + 8.0f);
    ImGui::TextColored(
        ImVec4(0.62f, 0.67f, 0.76f, 1.0f),
        "%d enabled  |  %d total  |  %s  (Drag & drop .jar files anywhere)",
        enabledCount, static_cast<int>(g_Launcher.mods.size()), FormatFileSize(totalBytes).c_str());

    curY += 46.0f;

    // Build filtered indices
    std::string q = LowerStr(searchBuf);
    std::vector<size_t> filteredIndices;
    filteredIndices.reserve(g_Launcher.mods.size());
    for (size_t i = 0; i < g_Launcher.mods.size(); ++i)
    {
        const auto &m = g_Launcher.mods[i];
        if (statusFilter == 1 && !m.enabled)
            continue;
        if (statusFilter == 2 && m.enabled)
            continue;
        if (!q.empty())
        {
            if (LowerStr(m.displayName).find(q) == std::string::npos &&
                LowerStr(m.modId).find(q) == std::string::npos &&
                LowerStr(m.fileName).find(q) == std::string::npos)
                continue;
        }
        filteredIndices.push_back(i);
    }

    float listH = std::max(120.0f, pageH - curY - 22.0f);
    ImGui::SetCursorPos(ImVec2(padX, curY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::BgSurface);
    ImGui::BeginChild("##ModsList", ImVec2(contentW, listH), true);

    if (filteredIndices.empty())
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImVec2 boxMin(ImGui::GetCursorScreenPos().x + 24.0f, ImGui::GetCursorScreenPos().y + 24.0f);
        ImVec2 boxMax(boxMin.x + avail.x - 48.0f, boxMin.y + std::max(160.0f, avail.y - 48.0f));

        draw->AddRectFilled(boxMin, boxMax, UITheme::BgInput, 12.0f);
        draw->AddRect(boxMin, boxMax, UITheme::BorderLight, 12.0f, 0, 1.5f);

        ImVec2 center((boxMin.x + boxMax.x) * 0.5f, (boxMin.y + boxMax.y) * 0.5f - 26.0f);
        UITheme::DrawVectorIcon(draw, UITheme::IconType::DropJar, center, 44.0f, UITheme::ForgeAmber);

        const char *emptyTitle = g_Launcher.mods.empty()
                                     ? "No .jar mods installed yet"
                                     : "No mods match your current filter";
        const char *emptySub = "Drop Forge mod .jar files here or click 'ADD MOD (.JAR)' above.";

        if (g_FontBold) ImGui::PushFont(g_FontBold);
        ImVec2 t1 = ImGui::CalcTextSize(emptyTitle);
        draw->AddText(ImVec2(center.x - t1.x * 0.5f, center.y + 34.0f), UITheme::TextPrimary, emptyTitle);
        if (g_FontBold) ImGui::PopFont();

        if (g_FontSmall) ImGui::PushFont(g_FontSmall);
        ImVec2 t2 = ImGui::CalcTextSize(emptySub);
        draw->AddText(ImVec2(center.x - t2.x * 0.5f, center.y + 58.0f), UITheme::TextSecondary, emptySub);
        if (g_FontSmall) ImGui::PopFont();
    }
    else
    {
        bool needsRescanAfterDelete = false;
        int rowHeight = 84;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filteredIndices.size()), static_cast<float>(rowHeight + 8));

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                size_t modIdx = filteredIndices[static_cast<size_t>(row)];
                ModInfo &mod = g_Launcher.mods[modIdx];

                // Lazy-upload embedded mod icon PNG once on UI thread
                if (!mod.textureAttempted && !mod.iconBytes.empty())
                {
                    mod.textureAttempted = true;
                    auto tex = OLIA_ENGINE::LoadUITextureFromMemory(mod.iconBytes.data(), mod.iconBytes.size());
                    mod.textureId = tex.id;
                    mod.iconBytes.clear();
                    mod.iconBytes.shrink_to_fit();
                }

                ImGui::PushID(static_cast<int>(modIdx));

                ImVec2 rowMin = ImGui::GetCursorScreenPos();
                float rowW = ImGui::GetContentRegionAvail().x;
                ImVec2 rowMax(rowMin.x + rowW, rowMin.y + static_cast<float>(rowHeight));
                ImDrawList *draw = ImGui::GetWindowDrawList();

                ImU32 cardBg = mod.enabled ? UITheme::BgSurfaceElevated : IM_COL32(18, 21, 30, 220);
                draw->AddRectFilled(rowMin, rowMax, cardBg, 10.0f);
                draw->AddRect(rowMin, rowMax, mod.enabled ? UITheme::BorderSubtle : IM_COL32(255, 255, 255, 12), 10.0f, 0, 1.0f);

                // Mod Icon (48x48)
                float icSize = 48.0f;
                ImVec2 icMin(rowMin.x + 14.0f, rowMin.y + (rowHeight - icSize) * 0.5f);
                ImVec2 icMax(icMin.x + icSize, icMin.y + icSize);
                if (mod.textureId != 0)
                {
                    draw->AddImageRounded(
                        static_cast<ImTextureID>(mod.textureId),
                        icMin, icMax, ImVec2(0, 0), ImVec2(1, 1),
                        mod.enabled ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 160, 180),
                        8.0f);
                }
                else
                {
                    draw->AddRectFilled(icMin, icMax, UITheme::BgInput, 8.0f);
                    ImU32 icCol = (mod.kind == JarKind::ForgeMod || mod.kind == JarKind::NeoForgeMod)
                                      ? UITheme::ForgeAmber
                                      : (mod.kind == JarKind::FabricMod ? UITheme::BlueAccent : UITheme::TextSecondary);
                    UITheme::DrawVectorIcon(draw, UITheme::IconType::Cube,
                                            ImVec2(icMin.x + icSize * 0.5f, icMin.y + icSize * 0.5f), 24.0f, icCol);
                }

                // Metadata text
                float textMaxW = std::max(200.0f, rowW - 280.0f);
                ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 76.0f, rowMin.y + 10.0f));
                ImGui::BeginGroup();

                if (g_FontBold) ImGui::PushFont(g_FontBold);
                ImGui::TextColored(
                    mod.enabled ? ImVec4(0.96f, 0.97f, 0.99f, 1.0f) : ImVec4(0.55f, 0.58f, 0.65f, 1.0f),
                    "%s", mod.displayName.c_str());
                if (g_FontBold) ImGui::PopFont();

                if (!mod.version.empty())
                {
                    ImGui::SameLine(0.0f, 8.0f);
                    UITheme::DrawBadge(("v" + mod.version).c_str(), IM_COL32(255, 255, 255, 18), UITheme::TextSecondary, 20.0f, 8.0f);
                }

                ImGui::SameLine(0.0f, 8.0f);
                if (mod.kind == JarKind::ForgeMod)
                    UITheme::DrawBadge("FORGE", UITheme::ForgeAmberSoft, UITheme::ForgeAmber, 20.0f, 8.0f);
                else if (mod.kind == JarKind::NeoForgeMod)
                    UITheme::DrawBadge("NEOFORGE", UITheme::ForgeAmberSoft, UITheme::ForgeAmber, 20.0f, 8.0f);
                else if (mod.kind == JarKind::FabricMod)
                    UITheme::DrawBadge("FABRIC (NEEDS FABRIC)", UITheme::BlueSoft, UITheme::BlueAccent, 20.0f, 8.0f);
                else
                    UITheme::DrawBadge("JAR", IM_COL32(255, 255, 255, 18), UITheme::TextSecondary, 20.0f, 8.0f);

                if (!mod.mcVersion.empty())
                {
                    ImGui::SameLine(0.0f, 6.0f);
                    std::string mcBadge = "MC " + mod.mcVersion;
                    UITheme::DrawBadge(mcBadge.c_str(), UITheme::EmeraldSoft, UITheme::Emerald, 20.0f, 8.0f);
                }

                if (g_FontSmall) ImGui::PushFont(g_FontSmall);
                std::string descLine = !mod.description.empty() ? mod.description : mod.fileName;
                if (descLine.size() > 96)
                    descLine = descLine.substr(0, 93) + "...";
                ImGui::TextColored(ImVec4(0.68f, 0.72f, 0.80f, 1.0f), "%s", descLine.c_str());

                std::string subLine = mod.fileName + "  (" + FormatFileSize(mod.fileSize) + ")";
                if (!mod.authors.empty())
                    subLine += "  -  by " + mod.authors;
                if (subLine.size() > static_cast<size_t>(textMaxW / 6.5f))
                    subLine = subLine.substr(0, static_cast<size_t>(textMaxW / 6.5f)) + "...";
                ImGui::TextColored(ImVec4(0.45f, 0.50f, 0.58f, 1.0f), "%s", subLine.c_str());
                if (g_FontSmall) ImGui::PopFont();

                ImGui::EndGroup();

                // Right Controls: Enabled Switch + Delete
                ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 166.0f, rowMin.y + 30.0f));
                if (g_FontSmall) ImGui::PushFont(g_FontSmall);
                ImGui::TextColored(
                    mod.enabled ? ImVec4(0.20f, 0.82f, 0.45f, 1.0f) : ImVec4(0.50f, 0.54f, 0.62f, 1.0f),
                    mod.enabled ? "ON " : "OFF");
                if (g_FontSmall) ImGui::PopFont();

                ImGui::SameLine(0.0f, 6.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
                bool toggled = mod.enabled;
                if (UITheme::DrawToggleSwitch("##ModEnableSwitch", &toggled))
                {
                    std::string err;
                    if (!ModManager::SetModEnabled(mod, toggled, err))
                    {
                        g_Launcher.SetStatus(err);
                    }
                    else
                    {
                        g_Launcher.SetStatus((toggled ? "Enabled " : "Disabled ") + mod.displayName);
                    }
                }

                ImGui::SameLine(0.0f, 14.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
                if (UITheme::DrawIconButton(
                        "##DeleteModBtn", "",
                        UITheme::IconType::Trash, ImVec2(34.0f, 34.0f),
                        UITheme::DangerSoft, IM_COL32(239, 68, 68, 80), UITheme::DangerRed,
                        UITheme::DangerRedHover, 8.0f))
                {
                    std::string err;
                    if (ModManager::DeleteModFile(mod, err))
                    {
                        g_Launcher.SetStatus("Deleted mod " + mod.fileName);
                        needsRescanAfterDelete = true;
                    }
                    else
                    {
                        g_Launcher.SetStatus(err);
                    }
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Delete mod .jar file");

                ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y + 8.0f));
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
                ImGui::PopID();
            }
        }

        if (needsRescanAfterDelete)
        {
            g_Launcher.modsNeedRescan = true;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}
