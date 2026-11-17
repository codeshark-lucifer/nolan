#pragma once

#include "ui/page.hpp"

struct SettingsPage : public IPage
{
public:
    void Draw() override;

private:
    static void DrawSidebar(float height);
    static void DrawGeneral();
    static void DrawJavaAndPerformance();
    static void DrawProfile();
    static void DrawLogs();
};
