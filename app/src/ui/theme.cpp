#include "ui/theme.hpp"
#include "state.hpp"

#include <algorithm>
#include <cmath>

namespace UITheme
{
    void ApplyModernStyle()
    {
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(16.0f, 16.0f);
        style.FramePadding = ImVec2(12.0f, 8.0f);
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.ScrollbarSize = 10.0f;
        style.GrabMinSize = 12.0f;

        style.WindowRounding = 10.0f;
        style.ChildRounding = 10.0f;
        style.FrameRounding = 8.0f;
        style.PopupRounding = 10.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 8.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;

        ImVec4 *colors = style.Colors;
        colors[ImGuiCol_Text]                 = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.50f, 0.58f, 1.00f);
        colors[ImGuiCol_WindowBg]             = ImVec4(0.05f, 0.055f, 0.08f, 1.00f);
        colors[ImGuiCol_ChildBg]              = ImVec4(0.08f, 0.095f, 0.14f, 0.96f);
        colors[ImGuiCol_PopupBg]              = ImVec4(0.09f, 0.11f, 0.16f, 0.98f);
        colors[ImGuiCol_Border]               = ImVec4(1.00f, 1.00f, 1.00f, 0.09f);
        colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]              = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.12f, 0.15f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.15f, 0.18f, 0.26f, 1.00f);
        colors[ImGuiCol_TitleBg]              = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgActive]        = ImVec4(0.08f, 0.10f, 0.15f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.05f, 0.06f, 0.09f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.24f, 0.29f, 0.39f, 0.85f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.39f, 0.51f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.13f, 0.77f, 0.37f, 1.00f);
        colors[ImGuiCol_CheckMark]            = ImVec4(0.13f, 0.77f, 0.37f, 1.00f);
        colors[ImGuiCol_SliderGrab]           = ImVec4(0.13f, 0.77f, 0.37f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.20f, 0.85f, 0.44f, 1.00f);
        colors[ImGuiCol_Button]               = ImVec4(0.13f, 0.16f, 0.23f, 1.00f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.18f, 0.22f, 0.31f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = ImVec4(0.10f, 0.13f, 0.19f, 1.00f);
        colors[ImGuiCol_Header]               = ImVec4(0.13f, 0.77f, 0.37f, 0.25f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(0.13f, 0.77f, 0.37f, 0.40f);
        colors[ImGuiCol_HeaderActive]         = ImVec4(0.13f, 0.77f, 0.37f, 0.55f);
        colors[ImGuiCol_Separator]            = ImVec4(1.00f, 1.00f, 1.00f, 0.08f);
        colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.02f, 0.03f, 0.05f, 0.75f);
    }

    void DrawVectorIcon(
        ImDrawList *draw,
        IconType icon,
        ImVec2 c,
        float size,
        ImU32 color)
    {
        float r = size * 0.5f;
        float t = std::max(1.5f, size * 0.11f);

        switch (icon)
        {
        case IconType::Play:
        {
            ImVec2 p1(c.x - r * 0.42f, c.y - r * 0.55f);
            ImVec2 p2(c.x - r * 0.42f, c.y + r * 0.55f);
            ImVec2 p3(c.x + r * 0.62f, c.y);
            draw->AddTriangleFilled(p1, p2, p3, color);
            break;
        }
        case IconType::Gear:
        {
            draw->AddCircle(c, r * 0.48f, color, 16, t);
            draw->AddCircleFilled(c, r * 0.18f, color, 12);
            for (int i = 0; i < 6; ++i)
            {
                float a = static_cast<float>(i) * 1.0471975f;
                ImVec2 p1(c.x + std::cos(a) * r * 0.52f, c.y + std::sin(a) * r * 0.52f);
                ImVec2 p2(c.x + std::cos(a) * r * 0.80f, c.y + std::sin(a) * r * 0.80f);
                draw->AddLine(p1, p2, color, t * 1.15f);
            }
            break;
        }
        case IconType::Cube:
        {
            ImVec2 top(c.x, c.y - r * 0.75f);
            ImVec2 mid(c.x, c.y);
            ImVec2 bot(c.x, c.y + r * 0.75f);
            ImVec2 tl(c.x - r * 0.68f, c.y - r * 0.36f);
            ImVec2 tr(c.x + r * 0.68f, c.y - r * 0.36f);
            ImVec2 bl(c.x - r * 0.68f, c.y + r * 0.38f);
            ImVec2 br(c.x + r * 0.68f, c.y + r * 0.38f);
            draw->AddQuad(top, tr, mid, tl, color, t * 0.85f);
            draw->AddQuad(tl, mid, bot, bl, color, t * 0.85f);
            draw->AddQuad(mid, tr, br, bot, color, t * 0.85f);
            break;
        }
        case IconType::Anvil:
        {
            // Stylized Forge anvil silhouette
            draw->AddRectFilled(ImVec2(c.x - r * 0.75f, c.y - r * 0.55f), ImVec2(c.x + r * 0.75f, c.y - r * 0.10f), color, 2.0f);
            draw->AddRectFilled(ImVec2(c.x - r * 0.32f, c.y - r * 0.12f), ImVec2(c.x + r * 0.32f, c.y + r * 0.35f), color);
            draw->AddRectFilled(ImVec2(c.x - r * 0.60f, c.y + r * 0.32f), ImVec2(c.x + r * 0.60f, c.y + r * 0.62f), color, 2.0f);
            break;
        }
        case IconType::Folder:
        {
            draw->AddRectFilled(ImVec2(c.x - r * 0.75f, c.y - r * 0.55f), ImVec2(c.x - r * 0.10f, c.y - r * 0.25f), color, 2.0f);
            draw->AddRect(ImVec2(c.x - r * 0.75f, c.y - r * 0.35f), ImVec2(c.x + r * 0.75f, c.y + r * 0.55f), color, 2.5f, 0, t);
            break;
        }
        case IconType::Search:
        {
            ImVec2 cc(c.x - r * 0.15f, c.y - r * 0.15f);
            draw->AddCircle(cc, r * 0.45f, color, 16, t);
            draw->AddLine(ImVec2(cc.x + r * 0.32f, cc.y + r * 0.32f), ImVec2(c.x + r * 0.68f, c.y + r * 0.68f), color, t * 1.15f);
            break;
        }
        case IconType::Refresh:
        {
            draw->PathArcTo(c, r * 0.55f, 0.5f, 5.2f, 16);
            draw->PathStroke(color, 0, t);
            ImVec2 tip(c.x + std::cos(5.2f) * r * 0.55f, c.y + std::sin(5.2f) * r * 0.55f);
            draw->AddTriangleFilled(
                ImVec2(tip.x - 4.0f, tip.y - 3.0f),
                ImVec2(tip.x + 4.0f, tip.y - 1.0f),
                ImVec2(tip.x + 1.0f, tip.y + 5.0f),
                color);
            break;
        }
        case IconType::Terminal:
        {
            draw->AddRect(ImVec2(c.x - r * 0.78f, c.y - r * 0.62f), ImVec2(c.x + r * 0.78f, c.y + r * 0.62f), color, 2.5f, 0, t);
            draw->AddLine(ImVec2(c.x - r * 0.42f, c.y - r * 0.22f), ImVec2(c.x - r * 0.10f, c.y), color, t);
            draw->AddLine(ImVec2(c.x - r * 0.10f, c.y), ImVec2(c.x - r * 0.42f, c.y + r * 0.22f), color, t);
            draw->AddLine(ImVec2(c.x + r * 0.05f, c.y + r * 0.22f), ImVec2(c.x + r * 0.42f, c.y + r * 0.22f), color, t);
            break;
        }
        case IconType::Trash:
        {
            draw->AddLine(ImVec2(c.x - r * 0.60f, c.y - r * 0.45f), ImVec2(c.x + r * 0.60f, c.y - r * 0.45f), color, t);
            draw->AddLine(ImVec2(c.x - r * 0.22f, c.y - r * 0.65f), ImVec2(c.x + r * 0.22f, c.y - r * 0.65f), color, t);
            draw->AddRect(ImVec2(c.x - r * 0.45f, c.y - r * 0.38f), ImVec2(c.x + r * 0.45f, c.y + r * 0.60f), color, 1.5f, 0, t);
            break;
        }
        case IconType::Plus:
        {
            draw->AddLine(ImVec2(c.x - r * 0.55f, c.y), ImVec2(c.x + r * 0.55f, c.y), color, t * 1.1f);
            draw->AddLine(ImVec2(c.x, c.y - r * 0.55f), ImVec2(c.x, c.y + r * 0.55f), color, t * 1.1f);
            break;
        }
        case IconType::Check:
        {
            draw->AddLine(ImVec2(c.x - r * 0.50f, c.y), ImVec2(c.x - r * 0.12f, c.y + r * 0.40f), color, t * 1.2f);
            draw->AddLine(ImVec2(c.x - r * 0.12f, c.y + r * 0.40f), ImVec2(c.x + r * 0.55f, c.y - r * 0.40f), color, t * 1.2f);
            break;
        }
        case IconType::DropJar:
        {
            draw->AddRect(ImVec2(c.x - r * 0.55f, c.y - r * 0.45f), ImVec2(c.x + r * 0.55f, c.y + r * 0.65f), color, 3.0f, 0, t);
            draw->AddRectFilled(ImVec2(c.x - r * 0.38f, c.y - r * 0.70f), ImVec2(c.x + r * 0.38f, c.y - r * 0.45f), color, 2.0f);
            draw->AddLine(ImVec2(c.x, c.y - r * 0.15f), ImVec2(c.x, c.y + r * 0.35f), color, t);
            draw->AddLine(ImVec2(c.x - r * 0.25f, c.y + r * 0.15f), ImVec2(c.x, c.y + r * 0.38f), color, t);
            draw->AddLine(ImVec2(c.x + r * 0.25f, c.y + r * 0.15f), ImVec2(c.x, c.y + r * 0.38f), color, t);
            break;
        }
        }
    }

    void DrawMinecraftBlockIcon(
        ImDrawList *draw,
        ImVec2 minPos,
        float size,
        bool isForge,
        bool isSnapshot)
    {
        ImVec2 maxPos(minPos.x + size, minPos.y + size);
        if (isForge)
        {
            draw->AddRectFilled(minPos, maxPos, IM_COL32(62, 39, 24, 255), 6.0f);
            draw->AddRect(minPos, maxPos, ForgeAmber, 6.0f, 0, 1.5f);
            DrawVectorIcon(draw, IconType::Anvil, ImVec2(minPos.x + size * 0.5f, minPos.y + size * 0.52f), size * 0.58f, ForgeAmber);
        }
        else if (isSnapshot)
        {
            draw->AddRectFilled(minPos, maxPos, IM_COL32(28, 52, 84, 255), 6.0f);
            draw->AddRect(minPos, maxPos, BlueAccent, 6.0f, 0, 1.5f);
            DrawVectorIcon(draw, IconType::Cube, ImVec2(minPos.x + size * 0.5f, minPos.y + size * 0.5f), size * 0.56f, IM_COL32(147, 197, 253, 255));
        }
        else
        {
            // Grass block aesthetic
            draw->AddRectFilled(minPos, maxPos, IM_COL32(107, 74, 48, 255), 6.0f);
            float grassH = size * 0.36f;
            draw->AddRectFilled(minPos, ImVec2(maxPos.x, minPos.y + grassH), IM_COL32(74, 186, 82, 255), 6.0f, ImDrawFlags_RoundCornersTop);
            draw->AddRectFilled(ImVec2(minPos.x + size * 0.18f, minPos.y + grassH - 1.0f),
                                ImVec2(minPos.x + size * 0.38f, minPos.y + grassH + size * 0.12f),
                                IM_COL32(74, 186, 82, 255));
            draw->AddRectFilled(ImVec2(minPos.x + size * 0.60f, minPos.y + grassH - 1.0f),
                                ImVec2(minPos.x + size * 0.82f, minPos.y + grassH + size * 0.10f),
                                IM_COL32(74, 186, 82, 255));
            draw->AddRect(minPos, maxPos, IM_COL32(255, 255, 255, 35), 6.0f, 0, 1.0f);
        }
    }

    void DrawBadge(
        const char *text,
        ImU32 bgColor,
        ImU32 textColor,
        float height,
        float horizontalPad)
    {
        if (!text || !text[0])
            return;
        ImFont *font = g_FontSmall ? g_FontSmall : g_FontRegular;
        if (font)
            ImGui::PushFont(font);

        ImVec2 textSize = ImGui::CalcTextSize(text);
        float w = textSize.x + horizontalPad * 2.0f;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList *draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + height), bgColor, height * 0.45f);
        draw->AddText(ImVec2(pos.x + horizontalPad, pos.y + (height - textSize.y) * 0.5f), textColor, text);
        ImGui::Dummy(ImVec2(w, height));

        if (font)
            ImGui::PopFont();
    }

    bool DrawToggleSwitch(const char *strId, bool *v)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList *draw = ImGui::GetWindowDrawList();

        constexpr float height = 24.0f;
        constexpr float width = 44.0f;
        constexpr float radius = height * 0.5f;

        ImGui::InvisibleButton(strId, ImVec2(width, height));
        bool clicked = ImGui::IsItemClicked();
        if (clicked)
            *v = !(*v);

        bool hovered = ImGui::IsItemHovered();
        ImU32 bgCol = *v ? (hovered ? EmeraldHover : Emerald) : (hovered ? IM_COL32(68, 76, 94, 255) : IM_COL32(50, 56, 72, 255));

        draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height), bgCol, radius);
        float knobX = *v ? (p.x + width - radius) : (p.x + radius);
        draw->AddCircleFilled(ImVec2(knobX, p.y + radius), radius - 3.0f, IM_COL32(255, 255, 255, 255), 20);
        return clicked;
    }

    bool DrawIconButton(
        const char *id,
        const char *label,
        IconType icon,
        ImVec2 size,
        ImU32 bg,
        ImU32 bgHover,
        ImU32 bgActive,
        ImU32 fg,
        float rounding)
    {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton(id, size);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImU32 col = active ? bgActive : (hovered ? bgHover : bg);
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), col, rounding);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), BorderSubtle, rounding, 0, 1.0f);

        float iconSize = std::min(16.0f, size.y * 0.45f);
        if (!label || !label[0])
        {
            DrawVectorIcon(draw, icon, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), iconSize, fg);
        }
        else
        {
            ImVec2 textSize = ImGui::CalcTextSize(label);
            float gap = 8.0f;
            float contentW = iconSize + gap + textSize.x;
            float startX = pos.x + (size.x - contentW) * 0.5f;
            DrawVectorIcon(draw, icon, ImVec2(startX + iconSize * 0.5f, pos.y + size.y * 0.5f), iconSize, fg);
            draw->AddText(ImVec2(startX + iconSize + gap, pos.y + (size.y - textSize.y) * 0.5f), fg, label);
        }
        return clicked;
    }

    void DrawCoverImage(
        ImDrawList *draw,
        ImTextureID tex,
        int texW,
        int texH,
        ImVec2 rectMin,
        ImVec2 rectMax,
        float rounding,
        ImU32 tint)
    {
        if (!tex || texW <= 0 || texH <= 0)
            return;

        float boxW = rectMax.x - rectMin.x;
        float boxH = rectMax.y - rectMin.y;
        if (boxW <= 1.0f || boxH <= 1.0f)
            return;

        float boxAspect = boxW / boxH;
        float texAspect = static_cast<float>(texW) / static_cast<float>(texH);

        ImVec2 uv0(0.0f, 0.0f);
        ImVec2 uv1(1.0f, 1.0f);

        if (texAspect > boxAspect)
        {
            float scale = boxAspect / texAspect;
            float pad = (1.0f - scale) * 0.5f;
            uv0.x = pad;
            uv1.x = 1.0f - pad;
        }
        else
        {
            float scale = texAspect / boxAspect;
            float pad = (1.0f - scale) * 0.5f;
            uv0.y = pad;
            uv1.y = 1.0f - pad;
        }

        draw->AddImageRounded(tex, rectMin, rectMax, uv0, uv1, tint, rounding);
    }

    void DrawProgressBarStyled(
        ImDrawList *draw,
        ImVec2 pos,
        ImVec2 size,
        float fraction,
        const char *label,
        ImU32 accentColor)
    {
        float f = std::clamp(fraction, 0.0f, 1.0f);
        ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
        draw->AddRectFilled(pos, maxPos, IM_COL32(25, 30, 42, 255), size.y * 0.35f);
        draw->AddRect(pos, maxPos, BorderLight, size.y * 0.35f, 0, 1.0f);

        if (f > 0.01f)
        {
            float fillW = std::max(size.y * 0.7f, size.x * f);
            draw->AddRectFilled(pos, ImVec2(pos.x + fillW, maxPos.y), accentColor, size.y * 0.35f);
        }

        if (label && label[0])
        {
            ImVec2 ts = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(pos.x + (size.x - ts.x) * 0.5f, pos.y + (size.y - ts.y) * 0.5f), TextPrimary, label);
        }
    }
} // namespace UITheme
