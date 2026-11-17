#pragma once

#include "ui/page.hpp"

struct PlayPage : public IPage
{
public:
    void Draw() override;

private:
    static void DrawHeroCard(float width, float height);
    static void DrawQuickStatsRow(float width);
    static void DrawLaunchDock(float width, float dockY);
    static void DrawVersionSelector(float width, float height);
    static void DrawPlayButton(float width, float height);
};
