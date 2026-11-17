#include "ui/forge_modal.hpp"
#include "ui/theme.hpp"
#include "actions.hpp"
#include "platform_utils.hpp"
#include "state.hpp"

#include <cstring>
#include <vector>

void ForgeModal::Draw()
{
    if (g_Launcher.showForgeInstallerModal)
    {
        ImGui::OpenPopup("Install Minecraft Forge");
    }

    ImGui::SetNextWindowSize(ImVec2(600.0f, 0.0f), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Install Minecraft Forge", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        if (g_FontTitle) ImGui::PushFont(g_FontTitle);
        ImGui::TextColored(ImVec4(0.95f, 0.60f, 0.22f, 1.0f), "Minecraft Forge Installer");
        if (g_FontTitle) ImGui::PopFont();
        ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "Install Forge automatically or import a forge-*-installer.jar file.");
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Tab Switcher
        ImGui::PushStyleColor(ImGuiCol_Button, g_Launcher.forgeModalTab == 0 ? UITheme::ForgeAmberSoft : UITheme::BgSurfaceElevated);
        ImGui::PushStyleColor(ImGuiCol_Text, g_Launcher.forgeModalTab == 0 ? UITheme::ForgeAmber : UITheme::TextSecondary);
        if (ImGui::Button("Official Online Forge", ImVec2(278.0f, 36.0f)))
            g_Launcher.forgeModalTab = 0;
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, g_Launcher.forgeModalTab == 1 ? UITheme::ForgeAmberSoft : UITheme::BgSurfaceElevated);
        ImGui::PushStyleColor(ImGuiCol_Text, g_Launcher.forgeModalTab == 1 ? UITheme::ForgeAmber : UITheme::TextSecondary);
        if (ImGui::Button("Local Installer (.jar)", ImVec2(278.0f, 36.0f)))
            g_Launcher.forgeModalTab = 1;
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        bool busy = g_Launcher.isBusy.load();

        if (g_Launcher.forgeModalTab == 0)
        {
            std::vector<ForgePromotionEntry> promos;
            {
                std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
                promos = g_Launcher.forgePromos;
            }

            if (promos.empty())
            {
                ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.84f, 1.0f), "%s Fetching official Forge builds...", GetSpinnerChar());
                ImGui::SameLine();
                if (ImGui::Button("Retry"))
                    RefreshForgePromotionsAsync();
            }
            else
            {
                if (g_Launcher.selectedForgePromoIndex >= static_cast<int>(promos.size()))
                    g_Launcher.selectedForgePromoIndex = 0;

                const auto &cur = promos[static_cast<size_t>(g_Launcher.selectedForgePromoIndex)];

                if (g_FontBold) ImGui::PushFont(g_FontBold);
                ImGui::Text("Minecraft Version");
                if (g_FontBold) ImGui::PopFont();
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##ForgeMcCombo", cur.mcVersion.c_str()))
                {
                    for (int i = 0; i < static_cast<int>(promos.size()); ++i)
                    {
                        bool sel = (i == g_Launcher.selectedForgePromoIndex);
                        std::string label = promos[static_cast<size_t>(i)].mcVersion;
                        if (!promos[static_cast<size_t>(i)].recommendedForge.empty())
                            label += "  (Recommended: " + promos[static_cast<size_t>(i)].recommendedForge + ")";
                        else if (!promos[static_cast<size_t>(i)].latestForge.empty())
                            label += "  (Latest: " + promos[static_cast<size_t>(i)].latestForge + ")";

                        if (ImGui::Selectable(label.c_str(), sel))
                            g_Launcher.selectedForgePromoIndex = i;
                    }
                    ImGui::EndCombo();
                }

                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                if (g_FontBold) ImGui::PushFont(g_FontBold);
                ImGui::Text("Forge Build Channel");
                if (g_FontBold) ImGui::PopFont();

                bool hasRec = !cur.recommendedForge.empty();
                bool hasLat = !cur.latestForge.empty();
                if (!hasRec && hasLat)
                    g_Launcher.preferRecommendedForge = false;

                ImGui::BeginDisabled(!hasRec);
                if (ImGui::RadioButton("Recommended (Stable)", g_Launcher.preferRecommendedForge))
                    g_Launcher.preferRecommendedForge = true;
                ImGui::EndDisabled();

                ImGui::SameLine(0.0f, 20.0f);
                ImGui::BeginDisabled(!hasLat);
                if (ImGui::RadioButton("Latest", !g_Launcher.preferRecommendedForge))
                    g_Launcher.preferRecommendedForge = false;
                ImGui::EndDisabled();

                std::string chosenForge = g_Launcher.preferRecommendedForge
                                              ? (!cur.recommendedForge.empty() ? cur.recommendedForge : cur.latestForge)
                                              : (!cur.latestForge.empty() ? cur.latestForge : cur.recommendedForge);

                ImGui::Dummy(ImVec2(0.0f, 8.0f));
                ImGui::TextColored(
                    ImVec4(0.68f, 0.74f, 0.82f, 1.0f),
                    "Target Profile: %s-forge-%s",
                    cur.mcVersion.c_str(), chosenForge.c_str());

                ImGui::Dummy(ImVec2(0.0f, 14.0f));
                ImGui::BeginDisabled(busy || chosenForge.empty());
                if (UITheme::DrawIconButton(
                        "##InstallOnlineForgeBtn",
                        busy ? "INSTALLING..." : "DOWNLOAD & INSTALL FORGE",
                        UITheme::IconType::Anvil,
                        ImVec2(270.0f, 40.0f),
                        UITheme::ForgeAmber, UITheme::ForgeAmberHover, UITheme::ForgeAmberActive,
                        IM_COL32(18, 12, 6, 255), 8.0f))
                {
                    InstallForgeOnlineAsync(cur.mcVersion, chosenForge);
                }
                ImGui::EndDisabled();
            }
        }
        else
        {
            if (g_FontBold) ImGui::PushFont(g_FontBold);
            ImGui::Text("Forge Installer .jar Path");
            if (g_FontBold) ImGui::PopFont();

            ImGui::SetNextItemWidth(430.0f);
            if (ImGui::InputTextWithHint("##LocalForgePath", "Select forge-*-installer.jar...", g_Launcher.localForgeJarPath, sizeof(g_Launcher.localForgeJarPath)))
            {
                g_Launcher.localForgeInspection = ForgeInstaller::InspectForgeInstallerJar(g_Launcher.localForgeJarPath);
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse...", ImVec2(124.0f, 34.0f)))
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
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            if (g_Launcher.localForgeJarPath[0] != '\0')
            {
                if (g_Launcher.localForgeInspection.valid)
                {
                    ImGui::TextColored(
                        ImVec4(0.25f, 0.85f, 0.45f, 1.0f),
                        "Detected %s Forge Installer: MC %s -> %s",
                        g_Launcher.localForgeInspection.isLegacy ? "Legacy" : "Modern",
                        g_Launcher.localForgeInspection.mcVersion.c_str(),
                        g_Launcher.localForgeInspection.targetVersionId.c_str());
                }
                else
                {
                    ImGui::TextColored(
                        ImVec4(0.95f, 0.40f, 0.40f, 1.0f),
                        "Not a valid Forge installer .jar (if this is a mod .jar, add it in the MODS tab!).");
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 14.0f));
            ImGui::BeginDisabled(busy || !g_Launcher.localForgeInspection.valid);
            if (UITheme::DrawIconButton(
                    "##InstallLocalForgeBtn",
                    busy ? "INSTALLING..." : "INSTALL FROM .JAR",
                    UITheme::IconType::Anvil,
                    ImVec2(240.0f, 40.0f),
                    UITheme::ForgeAmber, UITheme::ForgeAmberHover, UITheme::ForgeAmberActive,
                    IM_COL32(18, 12, 6, 255), 8.0f))
            {
                InstallForgeFromJarAsync(g_Launcher.localForgeJarPath);
            }
            ImGui::EndDisabled();
        }

        if (busy)
        {
            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            std::string st = g_Launcher.GetStatus();
            ImVec2 barPos = ImGui::GetCursorScreenPos();
            float barW = ImGui::GetContentRegionAvail().x;
            ImGui::Dummy(ImVec2(barW, 24.0f));
            UITheme::DrawProgressBarStyled(
                ImGui::GetWindowDrawList(),
                barPos,
                ImVec2(barW, 22.0f),
                g_Launcher.downloadProgress.load(),
                st.c_str(),
                UITheme::ForgeAmber);
        }

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        if (ImGui::Button("Close", ImVec2(110.0f, 36.0f)))
        {
            g_Launcher.showForgeInstallerModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
