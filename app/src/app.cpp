#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app.hpp"
#include "engine/engine.h"
#include "vulkan/vk_context.h"
#include "core/components/camera.h"
#include "core/components/transform.h"
#include "core/components/mesh-renderer.h"
#include "core/components/material.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <cstring>
#include <thread>
#include <atomic>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>

#include "launcher/launcher.hpp"

namespace fs = std::filesystem;

ImFont *g_FontRegular = nullptr;
ImFont *g_FontBold = nullptr;

// ============================================================================
// Global Launcher State
// ============================================================================
struct LauncherData
{
    // Profile State
    char username[64] = "Steve";
    char profileName[64] = "Steve";
    char accountType[64] = "Nolan Account"; // "Nolan Account", "Microsoft Account"
    bool isOnlineMode = false;
    PlayerProfile profile;

    // Navigation
    int currentPage = 0;   // 0 = Play, 1 = Installations, 2 = Settings
    int settingsPage = 0;  // 0 = General, 1 = Profile

    // General Settings
    bool closeAfterLaunch = false;
    bool checkForUpdates = true;
    bool useSystemLanguage = true;
    int ramAllocation = 4; // In GB
    char minecraftDirectory[512] = "";

    // Versions Data
    std::vector<VersionEntry> versions;
    int selectedVersion = 0;

    // Async Operations & Feedback
    std::atomic<bool> isBusy{false};
    std::atomic<float> downloadProgress{0.0f};
    std::string statusText = "Ready";

    // Microsoft Authentication
    MSADeviceCode msaCode;
    bool showAuthModal = false;
    std::atomic<bool> cancelAuth{false};
    bool isAuthenticated = false;

    // Confirm Dialog State
    bool showUninstallDialog = false;
    std::string versionToUninstall = "";
} g_Launcher;

// ============================================================================
// JSON Config Helper Functions (Saves in same directory as the .exe)
// ============================================================================
static std::string GetConfigFilePath()
{
    char path[MAX_PATH] = {0};
    DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (length > 0)
    {
        fs::path exePath(path);
        return (exePath.parent_path() / "data.json").string();
    }
    return "data.json";
}

static void SaveConfig()
{
    std::string filePath = GetConfigFilePath();
    std::ofstream file(filePath);
    if (!file.is_open())
        return;

    auto escapeJsonString = [](const std::string &str) {
        std::string res;
        for (char c : str) {
            if (c == '\\') res += "\\\\";
            else if (c == '"') res += "\\\"";
            else res += c;
        }
        return res;
    };

    file << "{\n";
    file << "  \"username\": \"" << escapeJsonString(g_Launcher.username) << "\",\n";
    file << "  \"profileName\": \"" << escapeJsonString(g_Launcher.profileName) << "\",\n";
    file << "  \"accountType\": \"" << escapeJsonString(g_Launcher.accountType) << "\",\n";
    file << "  \"isOnlineMode\": " << (g_Launcher.isOnlineMode ? "true" : "false") << ",\n";
    file << "  \"closeAfterLaunch\": " << (g_Launcher.closeAfterLaunch ? "true" : "false") << ",\n";
    file << "  \"checkForUpdates\": " << (g_Launcher.checkForUpdates ? "true" : "false") << ",\n";
    file << "  \"useSystemLanguage\": " << (g_Launcher.useSystemLanguage ? "true" : "false") << ",\n";
    file << "  \"ramAllocation\": " << g_Launcher.ramAllocation << ",\n";
    file << "  \"minecraftDirectory\": \"" << escapeJsonString(g_Launcher.minecraftDirectory) << "\",\n";
    file << "  \"selectedVersion\": " << g_Launcher.selectedVersion << "\n";
    file << "}\n";

    file.close();
}

static void LoadConfig()
{
    std::string filePath = GetConfigFilePath();
    std::ifstream file(filePath);
    if (!file.is_open())
        return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    auto getJsonString = [](const std::string &json, const std::string &key) -> std::string {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";

        size_t startQuote = json.find("\"", pos + searchKey.length());
        if (startQuote == std::string::npos) return "";

        size_t endQuote = json.find("\"", startQuote + 1);
        while (endQuote != std::string::npos && json[endQuote - 1] == '\\') {
            endQuote = json.find("\"", endQuote + 1);
        }
        if (endQuote == std::string::npos) return "";

        std::string raw = json.substr(startQuote + 1, endQuote - startQuote - 1);
        std::string unescaped;
        for (size_t i = 0; i < raw.length(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.length() && (raw[i + 1] == '\\' || raw[i + 1] == '"')) {
                unescaped += raw[i + 1];
                i++;
            } else {
                unescaped += raw[i];
            }
        }
        return unescaped;
    };

    auto getJsonBool = [](const std::string &json, const std::string &key, bool defaultVal) -> bool {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;

        size_t valStart = json.find_first_not_of(" \t\r\n", pos + searchKey.length());
        if (valStart == std::string::npos) return defaultVal;

        if (json.compare(valStart, 4, "true") == 0) return true;
        if (json.compare(valStart, 5, "false") == 0) return false;
        return defaultVal;
    };

    auto getJsonInt = [](const std::string &json, const std::string &key, int defaultVal) -> int {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultVal;

        size_t valStart = json.find_first_not_of(" \t\r\n", pos + searchKey.length());
        if (valStart == std::string::npos) return defaultVal;

        try {
            return std::stoi(json.substr(valStart));
        } catch (...) {
            return defaultVal;
        }
    };

    std::string valStr;

    valStr = getJsonString(json, "username");
    if (!valStr.empty()) std::strncpy(g_Launcher.username, valStr.c_str(), sizeof(g_Launcher.username) - 1);

    valStr = getJsonString(json, "profileName");
    if (!valStr.empty()) std::strncpy(g_Launcher.profileName, valStr.c_str(), sizeof(g_Launcher.profileName) - 1);

    valStr = getJsonString(json, "accountType");
    if (!valStr.empty()) std::strncpy(g_Launcher.accountType, valStr.c_str(), sizeof(g_Launcher.accountType) - 1);

    valStr = getJsonString(json, "minecraftDirectory");
    if (!valStr.empty()) std::strncpy(g_Launcher.minecraftDirectory, valStr.c_str(), sizeof(g_Launcher.minecraftDirectory) - 1);

    g_Launcher.isOnlineMode = getJsonBool(json, "isOnlineMode", g_Launcher.isOnlineMode);
    g_Launcher.closeAfterLaunch = getJsonBool(json, "closeAfterLaunch", g_Launcher.closeAfterLaunch);
    g_Launcher.checkForUpdates = getJsonBool(json, "checkForUpdates", g_Launcher.checkForUpdates);
    g_Launcher.useSystemLanguage = getJsonBool(json, "useSystemLanguage", g_Launcher.useSystemLanguage);

    g_Launcher.ramAllocation = getJsonInt(json, "ramAllocation", g_Launcher.ramAllocation);
    g_Launcher.selectedVersion = getJsonInt(json, "selectedVersion", g_Launcher.selectedVersion);
}

// Spinner indicator for loading states
static const char *GetSpinnerChar()
{
    static const char *frames[] = {"|", "/", "-", "\\"};
    int idx = (int)(ImGui::GetTime() * 8.0f) % 4;
    return frames[idx];
}

// Fetch versions asynchronously from Mojang / local disk
static void RefreshVersionsAsync()
{
    if (g_Launcher.isBusy)
        return;

    g_Launcher.isBusy = true;
    g_Launcher.statusText = "Fetching version list...";

    std::thread([]()
    {
        g_Launcher.versions = MinecraftLauncher::FetchVersions();
        if (g_Launcher.selectedVersion >= (int)g_Launcher.versions.size()) {
            g_Launcher.selectedVersion = 0;
        }
        g_Launcher.statusText = "Ready";
        g_Launcher.isBusy = false;
    }).detach();
}

// Launch Minecraft asynchronously
static void LaunchMinecraftAsync(const std::string &versionId)
{
    if (g_Launcher.isBusy)
        return;

    g_Launcher.isBusy = true;
    g_Launcher.statusText = "Preparing game files...";
    g_Launcher.downloadProgress = 0.0f;

    std::thread([versionId]()
    {
        PlayerProfile targetProfile;
        if (g_Launcher.isOnlineMode && g_Launcher.isAuthenticated)
        {
            targetProfile = g_Launcher.profile;
        }
        else
        {
            targetProfile = MinecraftLauncher::CreateOfflineProfile(g_Launcher.username);
        }

        bool launched = MinecraftLauncher::LaunchGame(
            targetProfile, versionId, g_Launcher.ramAllocation, g_Launcher.downloadProgress);

        if (launched)
        {
            g_Launcher.statusText = "Game Launched!";
            // Update local versions status
            g_Launcher.versions = MinecraftLauncher::FetchVersions();

            if (g_Launcher.closeAfterLaunch)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                exit(0);
            }
        }
        else
        {
            g_Launcher.statusText = "Failed to launch game!";
        }

        g_Launcher.isBusy = false;
    }).detach();
}

// Microsoft Device Code Authentication
static void StartMSALoginAsync()
{
    if (g_Launcher.isBusy)
        return;

    g_Launcher.isBusy = true;
    g_Launcher.statusText = "Requesting Microsoft Device Code...";

    std::thread([]()
    {
        if (MicrosoftAuth::RequestDeviceCode(g_Launcher.msaCode))
        {
            g_Launcher.showAuthModal = true;
            g_Launcher.cancelAuth = false;

            ShellExecuteA(NULL, "open", g_Launcher.msaCode.verificationUri.c_str(), NULL, NULL, SW_SHOWNORMAL);

            std::string authStatus = "";
            bool success = MicrosoftAuth::AuthenticateWithDeviceCode(
                g_Launcher.msaCode, g_Launcher.profile, g_Launcher.cancelAuth, authStatus);

            if (success)
            {
                g_Launcher.isAuthenticated = true;
                g_Launcher.isOnlineMode = true;
                std::strncpy(g_Launcher.username, g_Launcher.profile.username.c_str(), sizeof(g_Launcher.username) - 1);
                std::strncpy(g_Launcher.profileName, g_Launcher.profile.username.c_str(), sizeof(g_Launcher.profileName) - 1);
                std::strncpy(g_Launcher.accountType, "Microsoft Account", sizeof(g_Launcher.accountType) - 1);
                
                SaveConfig(); // Save profile when Microsoft authentication succeeds
            }

            g_Launcher.statusText = authStatus;
            g_Launcher.showAuthModal = false;
        }
        else
        {
            g_Launcher.statusText = "Failed to communicate with Microsoft Auth Server.";
        }

        g_Launcher.isBusy = false;
    }).detach();
}

// Interface Interface
struct IPage
{
    virtual void Draw() = 0;
};

// ============================================================================
// Header Component
// ============================================================================
struct Header
{
private:
    static void DrawAvatar(const char *username, float size = 48.0f)
    {
        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);

        // Circular background badge
        draw->AddCircleFilled(center, size * 0.5f, IM_COL32(70, 110, 190, 255));

        char letter[2] = {(username && username[0]) ? username[0] : '?', '\0'};
        if (letter[0] >= 'a' && letter[0] <= 'z') letter[0] -= 'a' - 'A';

        ImGui::PushFont(g_FontBold);
        ImVec2 textSize = ImGui::CalcTextSize(letter);
        draw->AddText(
            ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
            IM_COL32(255, 255, 255, 255), letter);
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(size, size));
    }

    static void DrawAccount()
    {
        ImGui::SetCursorPos(ImVec2(28.0f, 22.0f));

        DrawAvatar(g_Launcher.username, 48.0f);

        ImGui::SameLine(0.0f, 14.0f);
        ImGui::BeginGroup();

        ImGui::PushFont(g_FontBold);
        ImGui::TextUnformatted(g_Launcher.username);
        ImGui::PopFont();

        ImGui::PushFont(g_FontRegular);
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.0f), "%s", g_Launcher.accountType);
        ImGui::PopFont();

        ImGui::EndGroup();
    }

    static void DrawActions()
    {
        ImGuiIO &io = ImGui::GetIO();
        constexpr float buttonSize = 42.0f;
        constexpr float rightPadding = 28.0f;
        constexpr float spacing = 12.0f;

        float settingsX = io.DisplaySize.x - rightPadding - buttonSize;
        float notificationX = settingsX - spacing - buttonSize;

        ImGui::SetCursorPos(ImVec2(notificationX, 25.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 43, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(55, 59, 68, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 70, 80, 255));

        if (ImGui::Button("!", ImVec2(buttonSize, buttonSize)))
        {
            g_Launcher.statusText = "No new notifications.";
        }

        ImGui::SameLine(0.0f, spacing);

        if (ImGui::Button("S", ImVec2(buttonSize, buttonSize)))
        {
            g_Launcher.currentPage = 2; // Open Settings
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    static void DrawNavigation()
    {
        ImGuiIO &io = ImGui::GetIO();
        const char *playText = "PLAY";
        const char *installationsText = "INSTALLATIONS";

        ImVec2 padding(18.0f, 7.0f);
        float spacing = 6.0f;

        ImVec2 playTextSize = ImGui::CalcTextSize(playText);
        ImVec2 installationsTextSize = ImGui::CalcTextSize(installationsText);

        float playWidth = playTextSize.x + padding.x * 2.0f;
        float installationsWidth = installationsTextSize.x + padding.x * 2.0f;
        float totalWidth = playWidth + spacing + installationsWidth;

        float startX = (io.DisplaySize.x - totalWidth) * 0.5f;

        ImGui::SetCursorPos(ImVec2(startX, 86.0f));
        ImGui::PushFont(g_FontBold);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);

        if (g_Launcher.currentPage == 0)
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 75, 115, 255));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(35, 38, 45, 255));

        if (ImGui::Button("PLAY")) g_Launcher.currentPage = 0;
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, spacing);

        if (g_Launcher.currentPage == 1)
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 75, 115, 255));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(35, 38, 45, 255));

        if (ImGui::Button("INSTALLATIONS")) g_Launcher.currentPage = 1;
        ImGui::PopStyleColor();

        ImGui::PopStyleVar(2);
        ImGui::PopFont();
    }

public:
    static void Draw()
    {
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 125.0f), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("##LauncherHeader", nullptr, flags);

        DrawAccount();
        DrawActions();
        DrawNavigation();

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }
};

// ============================================================================
// Play Page Component
// ============================================================================
struct PlayPage : public IPage
{
private:
    static void DrawVersionSelector()
    {
        constexpr float width = 250.0f;
        constexpr float height = 52.0f;
        constexpr float popupHeight = 300.0f;
        constexpr float popupGap = 6.0f;

        ImDrawList *draw = ImGui::GetWindowDrawList();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(35, 35, 38, 235));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(48, 48, 52, 245));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(55, 55, 60, 255));

        bool clicked = ImGui::Button("##VersionSelector", ImVec2(width, height));
        ImVec2 buttonMin = ImGui::GetItemRectMin();
        ImVec2 buttonMax = ImGui::GetItemRectMax();

        if (clicked) ImGui::OpenPopup("VersionPopup");

        // Dynamic Label
        std::string selectedName = "Minecraft Release";
        std::string selectedVerId = "Select Version";

        if (!g_Launcher.versions.empty() && g_Launcher.selectedVersion < (int)g_Launcher.versions.size())
        {
            selectedVerId = g_Launcher.versions[g_Launcher.selectedVersion].id;
            selectedName = g_Launcher.versions[g_Launcher.selectedVersion].isDownloaded ? "Installed Release" : "Latest Release";
        }

        // Draw Block Icon
        constexpr float iconSize = 28.0f;
        ImVec2 iconPos(buttonMin.x + 10.0f, buttonMin.y + 12.0f);
        draw->AddRectFilled(iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize), IM_COL32(90, 170, 75, 255), 4.0f);

        // Version Title & Subtitle
        ImGui::PushFont(g_FontBold);
        draw->AddText(ImVec2(buttonMin.x + 48.0f, buttonMin.y + 6.0f), IM_COL32(245, 245, 245, 255), selectedName.c_str());
        ImGui::PopFont();

        ImGui::PushFont(g_FontRegular);
        draw->AddText(ImVec2(buttonMin.x + 48.0f, buttonMin.y + 28.0f), IM_COL32(170, 170, 175, 255), selectedVerId.c_str());
        ImGui::PopFont();

        // Arrow
        ImVec2 arrowCenter(buttonMax.x - 16.0f, buttonMin.y + height * 0.5f);
        draw->AddLine(ImVec2(arrowCenter.x - 4.0f, arrowCenter.y - 2.0f), ImVec2(arrowCenter.x, arrowCenter.y + 2.0f), IM_COL32(220, 220, 220, 255), 2.0f);
        draw->AddLine(ImVec2(arrowCenter.x, arrowCenter.y + 2.0f), ImVec2(arrowCenter.x + 4.0f, arrowCenter.y - 2.0f), IM_COL32(220, 220, 220, 255), 2.0f);

        if (ImGui::IsPopupOpen("VersionPopup"))
        {
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            float popupX = buttonMin.x;
            float popupY = buttonMax.y + popupGap;

            if (popupY + popupHeight > viewport->WorkPos.y + viewport->WorkSize.y)
                popupY = buttonMin.y - popupHeight - popupGap;

            ImGui::SetNextWindowPos(ImVec2(popupX, popupY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(width, popupHeight), ImGuiCond_Always);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(28, 29, 33, 250));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(70, 72, 78, 255));

            if (ImGui::BeginPopup("VersionPopup", ImGuiWindowFlags_NoMove))
            {
                ImGui::PushFont(g_FontRegular);
                ImGui::BeginChild("##VersionList", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

                for (int i = 0; i < (int)g_Launcher.versions.size(); ++i)
                {
                    bool selected = (g_Launcher.selectedVersion == i);
                    ImGui::PushID(i);

                    constexpr float itemHeight = 48.0f;
                    ImVec2 itemSize(ImGui::GetContentRegionAvail().x, itemHeight);

                    if (ImGui::Selectable("##VersionItem", selected, ImGuiSelectableFlags_None, itemSize))
                    {
                        g_Launcher.selectedVersion = i;
                        SaveConfig(); // Save selected version
                        ImGui::CloseCurrentPopup();
                    }

                    ImVec2 itemMin = ImGui::GetItemRectMin();
                    ImVec2 itemMax = ImGui::GetItemRectMax();
                    ImDrawList *popupDraw = ImGui::GetWindowDrawList();

                    if (selected)
                    {
                        popupDraw->AddRectFilled(itemMin, itemMax, IM_COL32(55, 95, 60, 180), 4.0f);
                    }

                    ImVec2 iconPos(itemMin.x + 8.0f, itemMin.y + 8.0f);
                    popupDraw->AddRectFilled(iconPos, ImVec2(iconPos.x + 32, iconPos.y + 32), IM_COL32(90, 170, 75, 255), 4.0f);

                    ImGui::PushFont(g_FontBold);
                    popupDraw->AddText(ImVec2(itemMin.x + 48.0f, itemMin.y + 4.0f), IM_COL32(245, 245, 245, 255), g_Launcher.versions[i].id.c_str());
                    ImGui::PopFont();

                    const char *statusStr = g_Launcher.versions[i].isDownloaded ? "Installed" : "Download";
                    popupDraw->AddText(ImVec2(itemMin.x + 48.0f, itemMin.y + 24.0f),
                                       g_Launcher.versions[i].isDownloaded ? IM_COL32(100, 210, 120, 255) : IM_COL32(165, 165, 170, 255),
                                       statusStr);

                    ImGui::PopID();
                }

                ImGui::EndChild();
                ImGui::PopFont();
                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    static void DrawPlayButton()
    {
        ImGui::PushFont(g_FontBold);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

        bool busy = g_Launcher.isBusy.load();

        if (busy)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 50, 50, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50, 50, 50, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 50, 50, 255));

            float progress = g_Launcher.downloadProgress.load();
            char label[64];
            if (progress > 0.0f)
                snprintf(label, sizeof(label), "%s DOWNLOADING %.0f%%", GetSpinnerChar(), progress * 100.0f);
            else
                snprintf(label, sizeof(label), "%s LAUNCHING...", GetSpinnerChar());

            ImGui::Button(label, ImVec2(220.0f, 52.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(20, 190, 70, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(35, 215, 85, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(15, 160, 55, 255));

            if (ImGui::Button("PLAY", ImVec2(220.0f, 52.0f)))
            {
                if (!g_Launcher.versions.empty())
                {
                    std::string verId = g_Launcher.versions[g_Launcher.selectedVersion].id;
                    LaunchMinecraftAsync(verId);
                }
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        ImGui::PopFont();
    }

    static void DrawQuickPlay()
    {
        ImGui::PushFont(g_FontBold);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 25));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 40));

        if (ImGui::Button("QUICK PLAY", ImVec2(130.0f, 52.0f)))
        {
            if (!g_Launcher.versions.empty())
            {
                // Launch first downloaded version or first version
                std::string targetVer = g_Launcher.versions[0].id;
                for (const auto &v : g_Launcher.versions)
                {
                    if (v.isDownloaded) { targetVer = v.id; break; }
                }
                LaunchMinecraftAsync(targetVer);
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        ImGui::PopFont();
    }

    static void DrawStatusBanner()
    {
        ImGui::PushFont(g_FontRegular);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.75f, 1.0f), "Status: %s", g_Launcher.statusText.c_str());
        ImGui::PopFont();
    }

public:
    void Draw() override
    {
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 125.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - 125.0f), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

        ImGui::Begin("##PlayPage", nullptr, flags);

        float bottomOffset = 42.0f;
        float controlHeight = 145.0f;
        float startY = io.DisplaySize.y - 125.0f - controlHeight - bottomOffset;

        float versionWidth = 250.0f;
        float playWidth = 220.0f;
        float quickWidth = 130.0f;
        float gap = 10.0f;

        float totalWidth = versionWidth + gap + playWidth + gap + quickWidth;
        float windowContentWidth = ImGui::GetContentRegionAvail().x;
        float startX = (windowContentWidth - totalWidth) * 0.5f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);
        ImGui::SetCursorPosY(startY);

        DrawVersionSelector();
        ImGui::SameLine(0.0f, gap);

        DrawPlayButton();
        ImGui::SameLine(0.0f, gap);

        DrawQuickPlay();

        // Status Feedback
        float statusX = (windowContentWidth - ImGui::CalcTextSize(g_Launcher.statusText.c_str()).x) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + statusX);
        ImGui::SetCursorPosY(startY + 68.0f);
        DrawStatusBanner();

        ImGui::End();
    }
};

// ============================================================================
// Installations Page Component
// ============================================================================
struct InstallationsPage : public IPage
{
public:
    void Draw() override
    {
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 125.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - 125.0f), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

        ImGui::Begin("##InstallationsPage", nullptr, flags);

        ImGui::SetCursorPos(ImVec2(30.0f, 20.0f));
        ImGui::PushFont(g_FontBold);
        ImGui::Text("INSTALLATIONS");
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(30.0f, 60.0f));
        ImGui::BeginChild("##InstallationsList", ImVec2(io.DisplaySize.x - 60.0f, io.DisplaySize.y - 200.0f), true);

        for (int i = 0; i < (int)g_Launcher.versions.size(); ++i)
        {
            const auto &ver = g_Launcher.versions[i];
            ImGui::PushID(i);

            ImGui::BeginGroup();
            ImGui::PushFont(g_FontBold);
            ImGui::Text("%s", ver.id.c_str());
            ImGui::PopFont();

            ImGui::PushFont(g_FontRegular);
            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.0f), "%s", ver.isDownloaded ? "Installed locally" : "Available on Mojang");
            ImGui::PopFont();
            ImGui::EndGroup();

            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180.0f);

            if (ver.isDownloaded)
            {
                if (ImGui::Button("PLAY", ImVec2(80.0f, 32.0f)))
                {
                    LaunchMinecraftAsync(ver.id);
                }
                ImGui::SameLine();
                if (ImGui::Button("REMOVE", ImVec2(80.0f, 32.0f)))
                {
                    g_Launcher.versionToUninstall = ver.id;
                    g_Launcher.showUninstallDialog = true;
                }
            }
            else
            {
                if (ImGui::Button("DOWNLOAD", ImVec2(165.0f, 32.0f)))
                {
                    LaunchMinecraftAsync(ver.id);
                }
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();

        // Uninstall Q&A Confirmation Modal
        if (g_Launcher.showUninstallDialog)
        {
            ImGui::OpenPopup("Uninstall Confirmation");
        }

        if (ImGui::BeginPopupModal("Uninstall Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Are you sure you want to uninstall version %s?", g_Launcher.versionToUninstall.c_str());
            ImGui::Text("This will delete local client files from your .minecraft folder.");
            ImGui::Spacing();

            if (ImGui::Button("Yes, Uninstall", ImVec2(120, 36)))
            {
                char *appData = std::getenv("APPDATA");
                if (appData)
                {
                    std::string verDir = std::string(appData) + "\\.minecraft\\versions\\" + g_Launcher.versionToUninstall;
                    std::error_code ec;
                    fs::remove_all(verDir, ec);
                    g_Launcher.versions = MinecraftLauncher::FetchVersions();
                }
                g_Launcher.showUninstallDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 36)))
            {
                g_Launcher.showUninstallDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
};

// ============================================================================
// Settings Page Component
// ============================================================================
struct SettingsPage : public IPage
{
private:
    static void DrawSidebar()
    {
        ImGuiIO &io = ImGui::GetIO();
        constexpr float sidebarWidth = 230.0f;

        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 15, 20, 255));

        ImGui::BeginChild("##SettingsSidebar", ImVec2(sidebarWidth, io.DisplaySize.y - 125.0f), false, ImGuiWindowFlags_NoScrollbar);

        ImGui::SetCursorPos(ImVec2(20.0f, 20.0f));
        ImGui::PushFont(g_FontBold);
        if (ImGui::Button("<  BACK", ImVec2(190.0f, 42.0f)))
        {
            g_Launcher.currentPage = 0;
        }
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 18.0f));
        ImGui::SetCursorPosX(24.0f);
        ImGui::PushFont(g_FontBold);
        ImGui::Text("SETTINGS");
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        // General
        ImGui::SetCursorPosX(12.0f);
        if (g_Launcher.settingsPage == 0)
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 75, 115, 255));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));

        if (ImGui::Button("GENERAL", ImVec2(206.0f, 44.0f))) g_Launcher.settingsPage = 0;
        ImGui::PopStyleColor();

        // Profile
        ImGui::SetCursorPosX(12.0f);
        if (g_Launcher.settingsPage == 1)
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 75, 115, 255));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));

        if (ImGui::Button("PROFILE", ImVec2(206.0f, 44.0f))) g_Launcher.settingsPage = 1;
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    static void DrawGeneral()
    {
        ImGui::PushFont(g_FontBold);
        ImGui::Text("General");
        ImGui::PopFont();
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.0f), "Configure launcher performance & path options.");
        ImGui::Dummy(ImVec2(0.0f, 18.0f));

        ImGui::Checkbox("Check for launcher updates", &g_Launcher.checkForUpdates);
        ImGui::Checkbox("Use system language", &g_Launcher.useSystemLanguage);
        ImGui::Checkbox("Close launcher after starting Minecraft", &g_Launcher.closeAfterLaunch);

        ImGui::Dummy(ImVec2(0.0f, 24.0f));
        ImGui::PushFont(g_FontBold);
        ImGui::Text("Minecraft Path");
        ImGui::PopFont();

        ImGui::SetNextItemWidth(500.0f);
        ImGui::InputText("##MinecraftDirectory", g_Launcher.minecraftDirectory, IM_ARRAYSIZE(g_Launcher.minecraftDirectory));

        ImGui::Dummy(ImVec2(0.0f, 24.0f));
        ImGui::PushFont(g_FontBold);
        ImGui::Text("Memory Allocation");
        ImGui::PopFont();

        ImGui::SetNextItemWidth(500.0f);
        ImGui::SliderInt("##RamAllocation", &g_Launcher.ramAllocation, 1, 16, "%d GB RAM");

        ImGui::Dummy(ImVec2(0.0f, 24.0f));
        if (ImGui::Button("SAVE SETTINGS", ImVec2(180.0f, 45.0f)))
        {
            SaveConfig();
            g_Launcher.statusText = "General settings saved!";
        }
    }

    static void DrawProfile()
    {
        ImGui::PushFont(g_FontBold);
        ImGui::Text("Profile Settings");
        ImGui::PopFont();
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.0f), "Configure user profile & Microsoft online authentication.");
        ImGui::Dummy(ImVec2(0.0f, 18.0f));

        ImGui::PushFont(g_FontBold);
        ImGui::Text("Profile Name");
        ImGui::PopFont();

        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputText("##ProfileName", g_Launcher.profileName, IM_ARRAYSIZE(g_Launcher.profileName));

        ImGui::Dummy(ImVec2(0.0f, 16.0f));
        ImGui::PushFont(g_FontBold);
        ImGui::Text("Account Type");
        ImGui::PopFont();

        const char *accountTypes[] = {"Nolan Account (Offline)", "Microsoft Account (Online)"};
        int currentIdx = g_Launcher.isOnlineMode ? 1 : 0;

        ImGui::SetNextItemWidth(400.0f);
        if (ImGui::Combo("##AccountTypeCombo", &currentIdx, accountTypes, IM_ARRAYSIZE(accountTypes)))
        {
            g_Launcher.isOnlineMode = (currentIdx == 1);
            std::strncpy(g_Launcher.accountType, accountTypes[currentIdx], sizeof(g_Launcher.accountType) - 1);
        }

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        if (g_Launcher.isOnlineMode)
        {
            if (g_Launcher.isAuthenticated)
            {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "Logged in as Microsoft User: %s", g_Launcher.profile.username.c_str());
            }
            else
            {
                if (ImGui::Button("LOG IN WITH MICROSOFT", ImVec2(220.0f, 42.0f)))
                {
                    StartMSALoginAsync();
                }
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        if (ImGui::Button("SAVE PROFILE", ImVec2(180.0f, 45.0f)))
        {
            std::strncpy(g_Launcher.username, g_Launcher.profileName, sizeof(g_Launcher.username) - 1);
            SaveConfig();
            g_Launcher.statusText = "Profile options saved!";
        }
    }

public:
    void Draw() override
    {
        ImGuiIO &io = ImGui::GetIO();
        constexpr float headerHeight = 125.0f;
        constexpr float sidebarWidth = 230.0f;

        ImGui::SetNextWindowPos(ImVec2(0.0f, headerHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - headerHeight), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

        ImGui::Begin("##SettingsPage", nullptr, flags);

        DrawSidebar();

        ImGui::SetCursorPos(ImVec2(sidebarWidth + 50.0f, 35.0f));
        ImGui::BeginChild("##SettingsContent", ImVec2(io.DisplaySize.x - sidebarWidth - 100.0f, io.DisplaySize.y - headerHeight - 70.0f));

        if (g_Launcher.settingsPage == 0)
            DrawGeneral();
        else
            DrawProfile();

        ImGui::EndChild();
        ImGui::End();
    }
};

// ============================================================================
// Main Application Callbacks
// ============================================================================

void setup()
{
    ImGui::SetCurrentContext(OLIA_ENGINE::GetImguiContext());

    ImGuiIO &io = ImGui::GetIO();
    g_FontRegular = io.Fonts->AddFontFromFileTTF("assets/fonts/default-regular.ttf", 18.0f);
    g_FontBold = io.Fonts->AddFontFromFileTTF("assets/fonts/deafult-bold.ttf", 18.0f);

    if (!g_FontRegular) g_FontRegular = io.FontDefault;
    if (!g_FontBold) g_FontBold = io.FontDefault;

    // Initialize Default Directory
    char *appData = std::getenv("APPDATA");
    if (appData)
    {
        std::string defDir = std::string(appData) + "\\.minecraft";
        std::strncpy(g_Launcher.minecraftDirectory, defDir.c_str(), sizeof(g_Launcher.minecraftDirectory) - 1);
    }

    // Load configuration directly from same folder as .exe
    LoadConfig();

    RefreshVersionsAsync();
}

void draw()
{
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
        SettingsPage page;
        page.Draw();
        break;
    }
    }

    // Microsoft Authentication Modal Dialog
    if (g_Launcher.showAuthModal)
    {
        ImGui::OpenPopup("Microsoft Device Login");
    }

    if (ImGui::BeginPopupModal("Microsoft Device Login", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("1. Open the following link in your browser:");
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%s", g_Launcher.msaCode.verificationUri.c_str());

        ImGui::Spacing();
        ImGui::Text("2. Enter this device code:");
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", g_Launcher.msaCode.userCode.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Copy Code & Open Link", ImVec2(200, 36)))
        {
            ImGui::SetClipboardText(g_Launcher.msaCode.userCode.c_str());
            ShellExecuteA(NULL, "open", g_Launcher.msaCode.verificationUri.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 36)))
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