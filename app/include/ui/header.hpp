#pragma once

struct Header
{
public:
    static constexpr float Height = 82.0f;
    static void Draw();

private:
    static void DrawBrandAndAccount(float headerHeight);
    static void DrawNavigation(float headerHeight);
    static void DrawActions(float headerHeight);
};
