#include "ui/header.hpp"
#include "ui/theme.hpp"
#include "actions.hpp"
#include "platform_utils.hpp"
#include "state.hpp"

#include <cstdio>
#include <string>

void Header::DrawBrandAndAccount(float headerHeight)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    float yCenter = headerHeight * 0.5f;

    // Brand Logo
    float logoSize = 36.0f;
    ImVec2 logoPos(24.0f, yCenter - logoSize * 0.5f);
    if (g_Textures.appIcon)
    {
        draw->AddImageRounded(
            g_Textures.appIcon,
            logoPos,
            ImVec2(logoPos.x + logoSize, logoPos.y + logoSize),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            IM_COL32(255, 255, 255, 255),
            8.0f);
    }
    else
    {
        UITheme::DrawMinecraftBlockIcon(draw, logoPos, logoSize, false, false);
    }

    ImGui::SetCursorPos(ImVec2(logoPos.x + logoSize + 12.0f, yCenter - 19.0f));
    ImGui::BeginGroup();
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "NOLAN");
    if (g_FontBold) ImGui::PopFont();

    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    ImGui::TextColored(ImVec4(0.20f, 0.82f, 0.45f, 1.0f), "MINECRAFT LAUNCHER");
    if (g_FontSmall) ImGui::PopFont();
    ImGui::EndGroup();

    // Account Pill (clickable -> goes to Profile Settings)
    float pillX = 248.0f;
    float pillW = 198.0f;
    float pillH = 44.0f;
    ImVec2 pillPos(pillX, yCenter - pillH * 0.5f);

    ImGui::SetCursorPos(pillPos);
    bool clicked = ImGui::InvisibleButton("##AccountPill", ImVec2(pillW, pillH));
    bool hovered = ImGui::IsItemHovered();
    if (clicked)
    {
        g_Launcher.currentPage = 3;
        g_Launcher.settingsPage = 2;
    }

    draw->AddRectFilled(
        pillPos,
        ImVec2(pillPos.x + pillW, pillPos.y + pillH),
        hovered ? UITheme::BgSurfaceHover : UITheme::BgSurfaceElevated,
        10.0f);
    draw->AddRect(
        pillPos,
        ImVec2(pillPos.x + pillW, pillPos.y + pillH),
        UITheme::BorderSubtle,
        10.0f, 0, 1.0f);

    // Avatar inside pill
    float avSize = 30.0f;
    ImVec2 avPos(pillPos.x + 8.0f, pillPos.y + (pillH - avSize) * 0.5f);
    if (g_Textures.profileAvatar)
    {
        draw->AddImageRounded(
            g_Textures.profileAvatar,
            avPos,
            ImVec2(avPos.x + avSize, avPos.y + avSize),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            IM_COL32(255, 255, 255, 255),
            6.0f);
    }
    else
    {
        ImVec2 avCenter(avPos.x + avSize * 0.5f, avPos.y + avSize * 0.5f);
        draw->AddCircleFilled(avCenter, avSize * 0.5f, IM_COL32(59, 130, 246, 255));
        char letter[2] = {(g_Launcher.username[0] != '\0') ? g_Launcher.username[0] : 'S', '\0'};
        if (letter[0] >= 'a' && letter[0] <= 'z') letter[0] -= 'a' - 'A';
        ImVec2 ts = ImGui::CalcTextSize(letter);
        draw->AddText(ImVec2(avCenter.x - ts.x * 0.5f, avCenter.y - ts.y * 0.5f), UITheme::TextPrimary, letter);
    }

    // Online/Offline dot
    bool onlineOk = g_Launcher.isOnlineMode && g_Launcher.isAuthenticated;
    ImU32 dotCol = onlineOk ? UITheme::Emerald : UITheme::ForgeAmber;
    draw->AddCircleFilled(ImVec2(avPos.x + avSize - 2.0f, avPos.y + avSize - 2.0f), 4.5f, UITheme::BgHeader);
    draw->AddCircleFilled(ImVec2(avPos.x + avSize - 2.0f, avPos.y + avSize - 2.0f), 3.0f, dotCol);

    if (g_FontBold) ImGui::PushFont(g_FontBold);
    draw->AddText(ImVec2(avPos.x + avSize + 10.0f, pillPos.y + 5.0f), UITheme::TextPrimary, g_Launcher.username);
    if (g_FontBold) ImGui::PopFont();

    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    draw->AddText(
        ImVec2(avPos.x + avSize + 10.0f, pillPos.y + 23.0f),
        UITheme::TextSecondary,
        onlineOk ? "Microsoft Online" : "Offline Profile");
    if (g_FontSmall) ImGui::PopFont();
}

void Header::DrawNavigation(float headerHeight)
{
    ImGuiIO &io = ImGui::GetIO();

    int enabledMods = 0;
    for (const auto &m : g_Launcher.mods)
    {
        if (m.enabled)
            enabledMods++;
    }

    char modsLabel[48];
    if (enabledMods > 0)
        snprintf(modsLabel, sizeof(modsLabel), "MODS (%d)", enabledMods);
    else
        snprintf(modsLabel, sizeof(modsLabel), "MODS");

    struct NavItem
    {
        const char *label;
        int pageIdx;
        UITheme::IconType icon;
        ImU32 accent;
    };

    NavItem items[] = {
        {"PLAY", 0, UITheme::IconType::Play, UITheme::Emerald},
        {"INSTALLATIONS", 1, UITheme::IconType::Anvil, UITheme::Emerald},
        {modsLabel, 2, UITheme::IconType::Cube, UITheme::ForgeAmber},
        {"SETTINGS", 3, UITheme::IconType::Gear, UITheme::BlueAccent}
    };

    if (g_FontBold) ImGui::PushFont(g_FontBold);

    float tabH = 40.0f;
    float gap = 6.0f;
    float totalW = 0.0f;
    float widths[4] = {};

    for (int i = 0; i < 4; ++i)
    {
        widths[i] = ImGui::CalcTextSize(items[i].label).x + 44.0f;
        totalW += widths[i];
    }
    totalW += gap * 3.0f;

    float minNavX = 464.0f;
    float maxRightReserved = 230.0f;
    float centeredX = (io.DisplaySize.x - totalW) * 0.5f;
    float startX = std::max(minNavX, std::min(centeredX, io.DisplaySize.x - maxRightReserved - totalW));
    float startY = (headerHeight - tabH) * 0.5f;

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        ImVec2(startX - 5.0f, startY - 4.0f),
        ImVec2(startX + totalW + 5.0f, startY + tabH + 4.0f),
        UITheme::BgInput,
        12.0f);
    draw->AddRect(
        ImVec2(startX - 5.0f, startY - 4.0f),
        ImVec2(startX + totalW + 5.0f, startY + tabH + 4.0f),
        UITheme::BorderSubtle,
        12.0f, 0, 1.0f);

    float curX = startX;
    for (int i = 0; i < 4; ++i)
    {
        bool active = (g_Launcher.currentPage == items[i].pageIdx);
        ImVec2 pos(curX, startY);
        ImVec2 sz(widths[i], tabH);

        ImGui::SetCursorPos(pos);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##NavTab", sz))
        {
            g_Launcher.currentPage = items[i].pageIdx;
            if (items[i].pageIdx == 2)
                g_Launcher.modsNeedRescan = true;
        }
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        if (active)
        {
            draw->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + sz.y), UITheme::BgSurfaceElevated, 9.0f);
            draw->AddRectFilled(
                ImVec2(pos.x + 12.0f, pos.y + sz.y - 3.0f),
                ImVec2(pos.x + sz.x - 12.0f, pos.y + sz.y),
                items[i].accent, 2.0f);
        }
        else if (hovered)
        {
            draw->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + sz.y), IM_COL32(255, 255, 255, 14), 9.0f);
        }

        ImU32 iconCol = active ? items[i].accent : UITheme::TextSecondary;
        ImU32 textCol = active ? UITheme::TextPrimary : (hovered ? UITheme::TextPrimary : UITheme::TextSecondary);
        ImVec2 ts = ImGui::CalcTextSize(items[i].label);
        float contentW = 14.0f + 8.0f + ts.x;
        float cx = pos.x + (sz.x - contentW) * 0.5f;

        UITheme::DrawVectorIcon(draw, items[i].icon, ImVec2(cx + 7.0f, pos.y + sz.y * 0.5f), 14.0f, iconCol);
        draw->AddText(ImVec2(cx + 22.0f, pos.y + (sz.y - ts.y) * 0.5f), textCol, items[i].label);

        curX += sz.x + gap;
    }

    if (g_FontBold) ImGui::PopFont();
}

void Header::DrawActions(float headerHeight)
{
    ImGuiIO &io = ImGui::GetIO();
    constexpr float btnSize = 38.0f;
    constexpr float gap = 8.0f;
    constexpr float rightPad = 24.0f;
    float y = (headerHeight - btnSize) * 0.5f;

    float xLog = io.DisplaySize.x - rightPad - btnSize;
    float xFolder = xLog - gap - btnSize;
    float xRefresh = xFolder - gap - btnSize;
    float xForge = xRefresh - gap - 116.0f;

    ImGui::SetCursorPos(ImVec2(xForge, y));
    if (g_FontSmall) ImGui::PushFont(g_FontSmall);
    if (UITheme::DrawIconButton(
            "##HdrForgeBtn", "GET FORGE",
            UITheme::IconType::Anvil, ImVec2(116.0f, btnSize),
            UITheme::ForgeAmberSoft, IM_COL32(238, 142, 45, 72), IM_COL32(238, 142, 45, 100),
            UITheme::ForgeAmber, 8.0f))
    {
        g_Launcher.showForgeInstallerModal = true;
        RefreshForgePromotionsAsync();
    }
    if (g_FontSmall) ImGui::PopFont();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Install Forge or import a forge-installer.jar");

    ImGui::SetCursorPos(ImVec2(xRefresh, y));
    if (UITheme::DrawIconButton(
            "##HdrRefreshBtn", "",
            UITheme::IconType::Refresh, ImVec2(btnSize, btnSize),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::TextPrimary, 8.0f))
    {
        RefreshVersionsAsync();
        g_Launcher.modsNeedRescan = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Refresh versions & mods");

    ImGui::SetCursorPos(ImVec2(xFolder, y));
    if (UITheme::DrawIconButton(
            "##HdrModsFolderBtn", "",
            UITheme::IconType::Folder, ImVec2(btnSize, btnSize),
            UITheme::BgSurfaceElevated, UITheme::BgSurfaceHover, UITheme::BgSurface,
            UITheme::TextPrimary, 8.0f))
    {
        PlatformUtils::OpenFolderInExplorer(g_Launcher.GetActiveModsDir());
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Open active mods/ folder in Explorer");

    ImGui::SetCursorPos(ImVec2(xLog, y));
    ImU32 logBg = g_Launcher.showConsoleDrawer ? UITheme::EmeraldSoft : UITheme::BgSurfaceElevated;
    ImU32 logFg = g_Launcher.showConsoleDrawer ? UITheme::Emerald : UITheme::TextPrimary;
    if (UITheme::DrawIconButton(
            "##HdrConsoleBtn", "",
            UITheme::IconType::Terminal, ImVec2(btnSize, btnSize),
            logBg, UITheme::BgSurfaceHover, UITheme::BgSurface,
            logFg, 8.0f))
    {
        g_Launcher.showConsoleDrawer = !g_Launcher.showConsoleDrawer;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Toggle Live Launcher & Minecraft Console");
}

void Header::Draw()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, Height), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UITheme::BgHeader);

    ImGui::Begin("##LauncherHeader", nullptr, flags);

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddLine(ImVec2(0.0f, Height - 1.0f), ImVec2(io.DisplaySize.x, Height - 1.0f), UITheme::BorderSubtle, 1.0f);

    DrawBrandAndAccount(Height);
    DrawNavigation(Height);
    DrawActions(Height);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
