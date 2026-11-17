#pragma once

#include <imgui.h>

namespace UITheme
{
    inline constexpr ImU32 BgCanvas          = IM_COL32(12, 14, 20, 255);
    inline constexpr ImU32 BgHeader          = IM_COL32(16, 19, 27, 250);
    inline constexpr ImU32 BgSurface         = IM_COL32(21, 25, 36, 248);
    inline constexpr ImU32 BgSurfaceElevated = IM_COL32(29, 35, 49, 252);
    inline constexpr ImU32 BgSurfaceHover    = IM_COL32(38, 46, 64, 255);
    inline constexpr ImU32 BgInput           = IM_COL32(15, 18, 26, 255);

    inline constexpr ImU32 BorderSubtle      = IM_COL32(255, 255, 255, 22);
    inline constexpr ImU32 BorderLight       = IM_COL32(255, 255, 255, 42);

    inline constexpr ImU32 Emerald           = IM_COL32(34, 197, 94, 255);
    inline constexpr ImU32 EmeraldHover      = IM_COL32(52, 218, 112, 255);
    inline constexpr ImU32 EmeraldActive     = IM_COL32(22, 163, 74, 255);
    inline constexpr ImU32 EmeraldSoft       = IM_COL32(34, 197, 94, 38);

    inline constexpr ImU32 ForgeAmber        = IM_COL32(238, 142, 45, 255);
    inline constexpr ImU32 ForgeAmberHover   = IM_COL32(248, 160, 68, 255);
    inline constexpr ImU32 ForgeAmberActive  = IM_COL32(204, 114, 28, 255);
    inline constexpr ImU32 ForgeAmberSoft    = IM_COL32(238, 142, 45, 40);

    inline constexpr ImU32 BlueAccent        = IM_COL32(59, 130, 246, 255);
    inline constexpr ImU32 BlueSoft          = IM_COL32(59, 130, 246, 40);

    inline constexpr ImU32 DangerRed         = IM_COL32(239, 68, 68, 255);
    inline constexpr ImU32 DangerRedHover    = IM_COL32(248, 92, 92, 255);
    inline constexpr ImU32 DangerSoft        = IM_COL32(239, 68, 68, 40);

    inline constexpr ImU32 TextPrimary       = IM_COL32(244, 246, 250, 255);
    inline constexpr ImU32 TextSecondary     = IM_COL32(156, 166, 186, 255);
    inline constexpr ImU32 TextMuted         = IM_COL32(106, 116, 136, 255);

    enum class IconType
    {
        Play,
        Gear,
        Cube,
        Anvil,
        Folder,
        Search,
        Refresh,
        Terminal,
        Trash,
        Plus,
        Check,
        DropJar
    };

    void ApplyModernStyle();

    void DrawVectorIcon(
        ImDrawList *draw,
        IconType icon,
        ImVec2 center,
        float size,
        ImU32 color);

    void DrawMinecraftBlockIcon(
        ImDrawList *draw,
        ImVec2 minPos,
        float size,
        bool isForge,
        bool isSnapshot);

    void DrawBadge(
        const char *text,
        ImU32 bgColor,
        ImU32 textColor,
        float height = 22.0f,
        float horizontalPad = 10.0f);

    bool DrawToggleSwitch(const char *strId, bool *v);

    bool DrawIconButton(
        const char *id,
        const char *label,
        IconType icon,
        ImVec2 size,
        ImU32 bg,
        ImU32 bgHover,
        ImU32 bgActive,
        ImU32 fg = TextPrimary,
        float rounding = 8.0f);

    void DrawCoverImage(
        ImDrawList *draw,
        ImTextureID tex,
        int texW,
        int texH,
        ImVec2 rectMin,
        ImVec2 rectMax,
        float rounding,
        ImU32 tint = IM_COL32(255, 255, 255, 255));

    void DrawProgressBarStyled(
        ImDrawList *draw,
        ImVec2 pos,
        ImVec2 size,
        float fraction,
        const char *label,
        ImU32 accentColor = Emerald);
} // namespace UITheme
