#include "ui/play_page.hpp"
#include "ui/header.hpp"
#include "ui/theme.hpp"
#include "actions.hpp"
#include "config.hpp"
#include "platform_utils.hpp"
#include "state.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace
{
    std::string ToLowerCopy(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }
} // namespace

void PlayPage::DrawHeroCard(float width, float height)
{
    ImVec2 pMin = ImGui::GetCursorScreenPos();
    ImVec2 pMax(pMin.x + width, pMin.y + height);
    ImDrawList *draw = ImGui::GetWindowDrawList();

    // Base card fill & artwork
    draw->AddRectFilled(pMin, pMax, UITheme::BgSurface, 14.0f);
    if (g_Textures.heroWall)
    {
        UITheme::DrawCoverImage(
            draw,
            g_Textures.heroWall,
            g_Textures.heroWallWidth,
            g_Textures.heroWallHeight,
            pMin, pMax, 14.0f,
            IM_COL32(255, 255, 255, 130));
    }

    // Sleek dark gradient overlay so foreground text pops clearly
    draw->AddRectFilledMultiColor(
        pMin, pMax,
        IM_COL32(12, 15, 23, 210),
        IM_COL32(14, 18, 28, 120),
        IM_COL32(12, 14, 21, 235),
        IM_COL32(12, 14, 21, 245));
    draw->AddRect(pMin, pMax, UITheme::BorderSubtle, 14.0f, 0, 1.0f);

    VersionEntry sel = g_Launcher.GetSelectedVersionCopy();
    if (sel.id.empty())
        sel.id = "Loading Versions...";

    int enabledMods = 0;
    for (const auto &m : g_Launcher.mods)
    {
        if (m.enabled)
            enabledMods++;
    }

    ImGui::BeginChild("##HeroContent", ImVec2(width, height), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(28.0f, 24.0f));
    ImGui::BeginGroup();

    // Top Badges
    if (sel.isModded)
    {
        std::string badgeLabel = sel.loaderName.empty() ? "FORGE MODDED" : sel.loaderName + " MODDED";
        for (char &c : badgeLabel)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        UITheme::DrawBadge(
            badgeLabel.c_str(),
            UITheme::ForgeAmberSoft,
            UITheme::ForgeAmber);
    }
    else if (sel.type == "snapshot")
    {
        UITheme::DrawBadge("SNAPSHOT BUILD", UITheme::BlueSoft, UITheme::BlueAccent);
    }
    else
    {
        UITheme::DrawBadge("MINECRAFT JAVA EDITION", UITheme::EmeraldSoft, UITheme::Emerald);
    }

    ImGui::SameLine(0.0f, 8.0f);
    if (sel.isDownloaded)
        UITheme::DrawBadge("INSTALLED LOCALLY", IM_COL32(34, 197, 94, 34), UITheme::Emerald);
    else
        UITheme::DrawBadge("READY TO DOWNLOAD", IM_COL32(255, 255, 255, 22), UITheme::TextSecondary);

    if (!sel.inheritsFrom.empty())
    {
        ImGui::SameLine(0.0f, 8.0f);
        std::string baseTag = "MC " + sel.inheritsFrom;
        UITheme::DrawBadge(baseTag.c_str(), IM_COL32(255, 255, 255, 18), UITheme::TextPrimary);
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // Hero Title
    if (g_FontHero) ImGui::PushFont(g_FontHero);
    ImGui::TextColored(ImVec4(0.97f, 0.98f, 1.0f, 1.0f), "%s", sel.id.c_str());
    if (g_FontHero) ImGui::PopFont();

    // Subtitle
    if (g_FontRegular) ImGui::PushFont(g_FontRegular);
    if (sel.isModded)
    {
        ImGui::TextColored(
            ImVec4(0.75f, 0.80f, 0.88f, 1.0f),
            "%s profile ready - %d active .jar mod(s) loaded from mods/",
            sel.loaderName.empty() ? "Modded" : sel.loaderName.c_str(),
            enabledMods);
    }
    else
    {
        ImGui::TextColored(
            ImVec4(0.72f, 0.77f, 0.85f, 1.0f),
            "Vanilla edition - install or switch to a Forge version to use .jar mods.");
    }
    if (g_FontRegular) ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // Hero Quick Action Pills
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (UITheme::DrawIconButton(
            "##HeroModsBtn", "MANAGE .JAR MODS",
            UITheme::IconType::Cube, ImVec2(190.0f, 38.0f),
            UITheme::ForgeAmberSoft, IM_COL32(238, 142, 45, 70), IM_COL32(238, 142, 45, 95),
            UITheme::ForgeAmber, 8.0f))
    {
        g_Launcher.currentPage = 2;
        g_Launcher.modsNeedRescan = true;
    }

    ImGui::SameLine(0.0f, 10.0f);
    if (UITheme::DrawIconButton(
            "##HeroForgeBtn", "INSTALL FORGE",
            UITheme::IconType::Anvil, ImVec2(164.0f, 38.0f),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::TextPrimary, 8.0f))
    {
        g_Launcher.showForgeInstallerModal = true;
        RefreshForgePromotionsAsync();
    }

    ImGui::SameLine(0.0f, 10.0f);
    if (UITheme::DrawIconButton(
            "##HeroDirBtn", "GAME FOLDER",
            UITheme::IconType::Folder, ImVec2(154.0f, 38.0f),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::TextPrimary, 8.0f))
    {
        PlatformUtils::OpenFolderInExplorer(g_Launcher.GetEffectiveGameDirForSelected());
    }
    if (g_FontBold) ImGui::PopFont();

    ImGui::EndGroup();
    ImGui::EndChild();
}

void PlayPage::DrawQuickStatsRow(float width)
{
    VersionEntry sel = g_Launcher.GetSelectedVersionCopy();
    int enabledMods = 0;
    int totalMods = static_cast<int>(g_Launcher.mods.size());
    for (const auto &m : g_Launcher.mods)
    {
        if (m.enabled)
            enabledMods++;
    }

    constexpr float gap = 14.0f;
    float cardW = (width - gap * 3.0f) * 0.25f;
    float cardH = 88.0f;
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    auto drawStatCard = [&](int index, const char *title, const std::string &value, const std::string &sub, UITheme::IconType icon, ImU32 accent, int clickTargetPage) {
        ImVec2 cMin(startPos.x + index * (cardW + gap), startPos.y);
        ImVec2 cMax(cMin.x + cardW, cMin.y + cardH);

        ImGui::SetCursorScreenPos(cMin);
        ImGui::PushID(index);
        bool clicked = ImGui::InvisibleButton("##StatCard", ImVec2(cardW, cardH));
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        if (clicked && clickTargetPage >= 0)
        {
            if (clickTargetPage == 99)
            {
                g_Launcher.showForgeInstallerModal = true;
                RefreshForgePromotionsAsync();
            }
            else
            {
                g_Launcher.currentPage = clickTargetPage;
                if (clickTargetPage == 2)
                    g_Launcher.modsNeedRescan = true;
            }
        }

        draw->AddRectFilled(cMin, cMax, hovered ? UITheme::BgSurfaceElevated : UITheme::BgSurface, 12.0f);
        draw->AddRect(cMin, cMax, hovered ? UITheme::BorderLight : UITheme::BorderSubtle, 12.0f, 0, 1.0f);

        // Icon badge on top-right
        ImVec2 icCenter(cMax.x - 24.0f, cMin.y + 24.0f);
        draw->AddCircleFilled(icCenter, 14.0f, IM_COL32(255, 255, 255, 14));
        UITheme::DrawVectorIcon(draw, icon, icCenter, 14.0f, accent);

        if (g_FontSmall) ImGui::PushFont(g_FontSmall);
        draw->AddText(ImVec2(cMin.x + 16.0f, cMin.y + 14.0f), UITheme::TextMuted, title);
        if (g_FontSmall) ImGui::PopFont();

        if (g_FontBold) ImGui::PushFont(g_FontBold);
        draw->AddText(ImVec2(cMin.x + 16.0f, cMin.y + 34.0f), UITheme::TextPrimary, value.c_str());
        if (g_FontBold) ImGui::PopFont();

        if (g_FontSmall) ImGui::PushFont(g_FontSmall);
        draw->AddText(ImVec2(cMin.x + 16.0f, cMin.y + 58.0f), UITheme::TextSecondary, sub.c_str());
        if (g_FontSmall) ImGui::PopFont();
    };

    std::string loaderVal = sel.isModded ? (sel.loaderName.empty() ? "Forge" : sel.loaderName) : "Vanilla";
    std::string loaderSub = sel.isModded
                                ? (sel.inheritsFrom.empty() ? "Modded Profile" : "Base MC " + sel.inheritsFrom)
                                : "Click to install Forge";
    drawStatCard(0, "MOD LOADER", loaderVal, loaderSub, UITheme::IconType::Anvil,
                 sel.isModded ? UITheme::ForgeAmber : UITheme::TextSecondary,
                 sel.isModded ? 1 : 99);

    std::string modsVal = std::to_string(enabledMods) + " Enabled";
    std::string modsSub = std::to_string(totalMods) + " total .jar file(s)";
    drawStatCard(1, "FORGE MODS (.JAR)", modsVal, modsSub, UITheme::IconType::Cube,
                 UITheme::ForgeAmber, 2);

    std::string ramVal = std::to_string(g_Launcher.ramAllocation) + " GB / " + std::to_string(g_Launcher.systemMemoryGB) + " GB";
    std::string javaSub = g_Launcher.javaRuntimes.empty()
                              ? "Using PATH java.exe"
                              : "Java " + std::to_string(g_Launcher.javaRuntimes[0].majorVersion) + " detected";
    drawStatCard(2, "MEMORY & JAVA", ramVal, javaSub, UITheme::IconType::Gear,
                 UITheme::BlueAccent, 3);

    bool online = g_Launcher.isOnlineMode && g_Launcher.isAuthenticated;
    std::string accVal = g_Launcher.username;
    std::string accSub = online ? "Microsoft Authenticated" : "Offline Nolan Account";
    drawStatCard(3, "ACTIVE PROFILE", accVal, accSub, UITheme::IconType::Check,
                 online ? UITheme::Emerald : UITheme::ForgeAmber, 3);

    ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + cardH));
}

void PlayPage::DrawVersionSelector(float width, float height)
{
    constexpr float popupHeight = 360.0f;
    constexpr float popupGap = 8.0f;
    static char versionSearch[64] = "";

    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    bool clicked = ImGui::InvisibleButton("##VersionSelector", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    ImVec2 buttonMin = ImGui::GetItemRectMin();
    ImVec2 buttonMax = ImGui::GetItemRectMax();

    if (clicked)
        ImGui::OpenPopup("VersionPopup");

    draw->AddRectFilled(
        buttonMin, buttonMax,
        hovered ? UITheme::BgSurfaceHover : UITheme::BgSurfaceElevated,
        10.0f);
    draw->AddRect(buttonMin, buttonMax, UITheme::BorderLight, 10.0f, 0, 1.0f);

    VersionEntry sel = g_Launcher.GetSelectedVersionCopy();
    std::string selectedTitle = sel.id.empty() ? "Select Version" : sel.id;
    std::string selectedSub = sel.isModded
                                  ? (sel.loaderName + (sel.isDownloaded ? " (Installed)" : ""))
                                  : (sel.isDownloaded ? "Installed Release" : "Available on Mojang");

    UITheme::DrawMinecraftBlockIcon(
        draw,
        ImVec2(pos.x + 12.0f, pos.y + (height - 34.0f) * 0.5f),
        34.0f,
        sel.isModded,
        sel.type == "snapshot");

    if (g_FontBold) ImGui::PushFont(g_FontBold);
    draw->AddText(ImVec2(pos.x + 56.0f, pos.y + 9.0f), UITheme::TextPrimary, selectedTitle.c_str());
    if (g_FontBold) ImGui::PopFont();

    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    draw->AddText(
        ImVec2(pos.x + 56.0f, pos.y + 31.0f),
        sel.isModded ? UITheme::ForgeAmber : (sel.isDownloaded ? UITheme::Emerald : UITheme::TextSecondary),
        selectedSub.c_str());
    if (g_FontSmall) ImGui::PopFont();

    ImVec2 arrowC(buttonMax.x - 18.0f, buttonMin.y + height * 0.5f);
    draw->AddTriangleFilled(
        ImVec2(arrowC.x - 5.0f, arrowC.y - 3.0f),
        ImVec2(arrowC.x + 5.0f, arrowC.y - 3.0f),
        ImVec2(arrowC.x, arrowC.y + 4.0f),
        UITheme::TextSecondary);

    if (ImGui::IsPopupOpen("VersionPopup"))
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        float popupW = std::max(width, 340.0f);
        float popupX = buttonMin.x;
        float popupY = buttonMin.y - popupHeight - popupGap;
        if (popupY < viewport->WorkPos.y + Header::Height)
            popupY = buttonMax.y + popupGap;

        ImGui::SetNextWindowPos(ImVec2(popupX, popupY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(popupW, popupHeight), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, UITheme::BgSurface);

        if (ImGui::BeginPopup("VersionPopup", ImGuiWindowFlags_NoMove))
        {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##VerPopupSearch", "Search versions (e.g. forge, 1.20.1)...", versionSearch, sizeof(versionSearch));
            ImGui::Dummy(ImVec2(0.0f, 4.0f));

            std::vector<VersionEntry> snapshotVersions;
            int currentSelIdx = 0;
            {
                std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
                snapshotVersions = g_Launcher.versions;
                currentSelIdx = g_Launcher.selectedVersion;
            }

            std::string q = ToLowerCopy(versionSearch);
            std::vector<int> filteredIndices;
            filteredIndices.reserve(snapshotVersions.size());
            for (int i = 0; i < static_cast<int>(snapshotVersions.size()); ++i)
            {
                if (q.empty() || ToLowerCopy(snapshotVersions[static_cast<size_t>(i)].id).find(q) != std::string::npos)
                    filteredIndices.push_back(i);
            }

            ImGui::BeginChild("##VersionPopupList", ImVec2(0.0f, 0.0f), false);
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(filteredIndices.size()), 50.0f);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    int i = filteredIndices[static_cast<size_t>(row)];
                    const auto &ver = snapshotVersions[static_cast<size_t>(i)];
                    bool selected = (currentSelIdx == i);

                    ImGui::PushID(i);
                    ImVec2 itemSize(ImGui::GetContentRegionAvail().x, 46.0f);
                    if (ImGui::Selectable("##VerPopItem", selected, ImGuiSelectableFlags_None, itemSize))
                    {
                        {
                            std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
                            g_Launcher.selectedVersion = i;
                            g_Launcher.selectedVersionId = ver.id;
                            g_Launcher.modsNeedRescan = true;
                        }
                        SaveConfig();
                        ImGui::CloseCurrentPopup();
                    }

                    ImVec2 iMin = ImGui::GetItemRectMin();
                    ImVec2 iMax = ImGui::GetItemRectMax();
                    ImDrawList *pDraw = ImGui::GetWindowDrawList();

                    if (selected)
                        pDraw->AddRectFilled(iMin, iMax, UITheme::EmeraldSoft, 8.0f);

                    UITheme::DrawMinecraftBlockIcon(
                        pDraw,
                        ImVec2(iMin.x + 8.0f, iMin.y + 8.0f),
                        30.0f,
                        ver.isModded,
                        ver.type == "snapshot");

                    if (g_FontBold) ImGui::PushFont(g_FontBold);
                    pDraw->AddText(ImVec2(iMin.x + 48.0f, iMin.y + 5.0f), UITheme::TextPrimary, ver.id.c_str());
                    if (g_FontBold) ImGui::PopFont();

                    const char *sub = ver.isModded ? "Forge / Modded" : (ver.isDownloaded ? "Installed" : "Click to download");
                    ImU32 subCol = ver.isModded ? UITheme::ForgeAmber : (ver.isDownloaded ? UITheme::Emerald : UITheme::TextSecondary);
                    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
                    pDraw->AddText(ImVec2(iMin.x + 48.0f, iMin.y + 25.0f), subCol, sub);
                    if (g_FontSmall) ImGui::PopFont();

                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

void PlayPage::DrawPlayButton(float width, float height)
{
    bool busy = g_Launcher.isBusy.load();
    if (g_FontBold) ImGui::PushFont(g_FontBold);

    if (busy)
    {
        float progress = g_Launcher.downloadProgress.load();
        char label[64];
        if (progress > 0.0f && progress < 1.0f)
            snprintf(label, sizeof(label), "%s PREPARING %.0f%%", GetSpinnerChar(), progress * 100.0f);
        else
            snprintf(label, sizeof(label), "%s WORKING...", GetSpinnerChar());

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, height));
        UITheme::DrawProgressBarStyled(ImGui::GetWindowDrawList(), pos, ImVec2(width, height), progress, label, UITheme::Emerald);
    }
    else
    {
        if (UITheme::DrawIconButton(
                "##PlayMainBtn", "PLAY",
                UITheme::IconType::Play, ImVec2(width, height),
                UITheme::Emerald, UITheme::EmeraldHover, UITheme::EmeraldActive,
                IM_COL32(8, 20, 12, 255), 10.0f))
        {
            VersionEntry sel = g_Launcher.GetSelectedVersionCopy();
            if (!sel.id.empty())
            {
                LaunchMinecraftAsync(sel.id);
            }
        }
    }

    if (g_FontBold) ImGui::PopFont();
}

void PlayPage::DrawLaunchDock(float width, float dockY)
{
    constexpr float dockH = 92.0f;
    ImVec2 dockPos(ImGui::GetCursorPosX(), dockY);
    ImGui::SetCursorPos(dockPos);

    ImVec2 screenMin = ImGui::GetCursorScreenPos();
    ImVec2 screenMax(screenMin.x + width, screenMin.y + dockH);
    ImDrawList *draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(screenMin, screenMax, UITheme::BgSurface, 14.0f);
    draw->AddRect(screenMin, screenMax, UITheme::BorderSubtle, 14.0f, 0, 1.0f);

    float innerY = dockPos.y + 18.0f;
    float selectorW = std::min(340.0f, width * 0.36f);
    float playW = 220.0f;
    float modsW = 136.0f;

    // Left: Version Selector
    ImGui::SetCursorPos(ImVec2(dockPos.x + 18.0f, innerY));
    DrawVersionSelector(selectorW, 56.0f);

    // Center: Status & Progress bar
    float statusX = dockPos.x + 18.0f + selectorW + 18.0f;
    float rightControlsW = playW + 12.0f + modsW + 18.0f;
    float statusW = std::max(120.0f, width - (selectorW + 36.0f + rightControlsW));

    std::string status = g_Launcher.GetStatus();
    bool busy = g_Launcher.isBusy.load();
    float progress = g_Launcher.downloadProgress.load();

    ImGui::SetCursorPos(ImVec2(statusX, innerY + 4.0f));
    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    draw->AddText(
        ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y),
        busy ? UITheme::Emerald : UITheme::TextSecondary,
        status.c_str());
    if (g_FontSmall) ImGui::PopFont();

    ImVec2 barScreen(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 24.0f);
    UITheme::DrawProgressBarStyled(
        draw, barScreen, ImVec2(statusW, 10.0f),
        busy ? progress : 0.0f, "", UITheme::Emerald);

    // Right: PLAY + MODS buttons
    float playX = dockPos.x + width - 18.0f - modsW - 12.0f - playW;
    ImGui::SetCursorPos(ImVec2(playX, innerY));
    DrawPlayButton(playW, 56.0f);

    ImGui::SetCursorPos(ImVec2(playX + playW + 12.0f, innerY));
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    if (UITheme::DrawIconButton(
            "##DockModsBtn", "MODS",
            UITheme::IconType::Cube, ImVec2(modsW, 56.0f),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::ForgeAmber, 10.0f))
    {
        g_Launcher.currentPage = 2;
        g_Launcher.modsNeedRescan = true;
    }
    if (g_FontBold) ImGui::PopFont();
}

void PlayPage::Draw()
{
    ImGuiIO &io = ImGui::GetIO();
    float consoleHeight = g_Launcher.showConsoleDrawer ? 210.0f : 0.0f;
    float pageY = Header::Height;
    float pageH = std::max(200.0f, io.DisplaySize.y - Header::Height - consoleHeight);

    ImGui::SetNextWindowPos(ImVec2(0.0f, pageY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, pageH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##PlayPage", nullptr, flags);

    constexpr float padX = 28.0f;
    constexpr float padY = 20.0f;
    float contentW = io.DisplaySize.x - padX * 2.0f;
    float dockH = 92.0f;
    float statsH = 88.0f;
    float heroH = std::max(210.0f, pageH - padY * 2.0f - dockH - statsH - 32.0f);

    ImGui::SetCursorPos(ImVec2(padX, padY));
    DrawHeroCard(contentW, heroH);

    ImGui::SetCursorPos(ImVec2(padX, padY + heroH + 14.0f));
    DrawQuickStatsRow(contentW);

    ImGui::SetCursorPosX(padX);
    DrawLaunchDock(contentW, pageH - dockH - padY);

    ImGui::End();
}
