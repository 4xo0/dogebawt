#include "menu.h"
#include "config.h"
#include "imgui.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>

namespace menu {
    namespace {
        bool s_open = false;
        DWORD s_lastToggle = 0;

        ImVec4 Color(const float (&v)[4]) { return ImVec4(v[0], v[1], v[2], v[3]); }

        const char* KeyName(int vk) {
            static char name[64];
            if (!vk) return "None";
            if (vk < 0) {
                static const char* mouse[] = { "LMB", "RMB", "MMB", "Mouse 4", "Mouse 5" };
                const int index = -vk - 1;
                return index >= 0 && index < 5 ? mouse[index] : "Mouse";
            }
            UINT scan = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC) << 16;
            if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN ||
                vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME ||
                vk == VK_INSERT || vk == VK_DELETE || vk == VK_DIVIDE || vk == VK_NUMLOCK)
                scan |= 1u << 24;
            if (GetKeyNameTextA(static_cast<LONG>(scan), name, sizeof(name)) > 0) return name;
            std::snprintf(name, sizeof(name), "0x%02X", vk);
            return name;
        }

        void HotkeyWidget(const char* id, Keybind& bind, float width = 100.0f) {
            ImGui::PushID(id);
            ImGui::SetNextItemWidth(width);
            const char* label = bind.listening ? "..." : KeyName(bind.vk);
            if (ImGui::Button(label, ImVec2(width, 0.0f))) bind.listening = true;

            if (bind.listening) {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                    bind.vk = 0;
                    bind.listening = false;
                } else {
                    for (int mb = 0; mb < 5 && bind.listening; ++mb) {
                        if (ImGui::IsMouseClicked(static_cast<ImGuiMouseButton>(mb))) {
                            bind.vk = -(mb + 1);
                            bind.listening = false;
                        }
                    }
                    for (int vk = 0x08; vk <= 0xFE && bind.listening; ++vk) {
                        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
                            vk == VK_XBUTTON1 || vk == VK_XBUTTON2) continue;
                        if (GetAsyncKeyState(vk) & 1) {
                            bind.vk = vk;
                            bind.listening = false;
                        }
                    }
                }
            }
            ImGui::PopID();
        }

        void LabelHotkey(const char* label, const char* id, Keybind& bind) {
            ImGui::TextUnformatted(label);
            ImGui::SameLine(145.0f);
            HotkeyWidget(id, bind);
        }

        void ModsTab() {
            ImGui::TextUnformatted("Mods");
            ImGui::Separator();
            ImGui::Checkbox("Use Speed 1 (Unticked Uses Speed 2)", &g_cfg.useSpeed1);
            ImGui::Checkbox("Show Current Speed", &g_cfg.showCurrentSpeed);
            ImGui::SliderFloat("Speedhack Speed 1", &g_cfg.speedhackSpeed1, 0.0f, 5.0f, "%.2f");
            ImGui::SliderFloat("Speedhack Speed 2", &g_cfg.speedhackSpeed2, 0.0f, 5.0f, "%.2f");
            ImGui::Checkbox("No Fog", &g_cfg.noFog);
            ImGui::Checkbox("SocketFu", &g_cfg.socketFu);
            ImGui::Checkbox("Show SocketFu Timer", &g_cfg.showSocketFuTimer);
            ImGui::Checkbox("Use Second Speed in SocketFu", &g_cfg.socketFuUseSecondSpeed);
            ImGui::Checkbox("Restrict My Movement Speed in SocketFu", &g_cfg.socketFuRestrictMovement);
            ImGui::Checkbox("NoClip When Socketed", &g_cfg.socketFuNoClip);
            ImGui::Checkbox("Auto NoClip (walk into walls)", &g_cfg.autoNoClip);
            ImGui::Checkbox("Show Current Fame Per Minute", &g_cfg.showFpm);
            ImGui::SliderFloat("Camera Zoom Scale", &g_cfg.cameraZoomScale, 0.25f, 5.0f, "%.2f");
            ImGui::Checkbox("Anti-Idle", &g_cfg.antiIdle);
        }

        void NexusTab() {
            ImGui::TextUnformatted("Auto Nexus Settings");
            ImGui::Separator();
            ImGui::TextWrapped("This is all experimental, auto nexus might fail, YOU MAY DIE!");
            ImGui::TextWrapped("This will detect and respond when server side hits happen.");
            ImGui::TextWrapped("This will NOT save you from server side hit death.");
            ImGui::TextWrapped("Not advised to set to low values because of server side hits.");
            ImGui::Spacing();
            ImGui::Checkbox("Auto Nexus Enabled", &g_cfg.autoNexus);
            ImGui::SliderFloat("Health Value", &g_cfg.autoNexusHpValue, 0.0f, 1000.0f, "%.0f");
            ImGui::Checkbox("Use Health Percent Instead of Value", &g_cfg.autoNexusUsePercent);
            ImGui::SliderFloat("Health Percent", &g_cfg.autoNexusHpPercent, 0.0f, 99.99f, "%.2f");
            ImGui::Checkbox("Display When Nexus'd", &g_cfg.autoNexusDisplay);
        }

        void AimTab() {
            ImGui::TextUnformatted("Auto Aim Settings");
            ImGui::Separator();
            LabelHotkey("Toggle Key", "aimbotHotkey", g_cfg.aimbotHotkey);
            ImGui::Checkbox("Auto Aim  ", &g_cfg.autoAim);
            static const char* styles[] = { "Distance", "Cursor", "Health" };
            ImGui::Combo("Targeting Style", &g_cfg.targetingStyle, styles, 3);
            ImGui::Checkbox("Magnet Aim", &g_cfg.magnetAim);
            ImGui::Checkbox("Magnet Aim Range Extension", &g_cfg.magnetRangeExt);
            ImGui::SliderFloat("\nMagnet Aim Range (Ctrl + Click to type)",
                               &g_cfg.magnetAimRange, 1.0f, 2.25f, "%.3f");
            ImGui::Checkbox("Projectile No Clip", &g_cfg.projectileNoClip);
            ImGui::Checkbox("Render Aim Info", &g_cfg.renderAimInfo);
        }

        void DodgeTab() {
            ImGui::TextUnformatted("Auto Dodge Settings");
            ImGui::Separator();
            ImGui::Checkbox("Dodge Projectiles", &g_cfg.dodgeProjectiles);
            ImGui::Checkbox("Hold To Toggle\t", &g_cfg.dodgeHoldToToggle);
            ImGui::SameLine();
            HotkeyWidget("dodgingHotkey", g_cfg.dodgingHotkey);
            ImGui::Checkbox("Dodge Invisible", &g_cfg.dodgeInvisible);
            ImGui::Checkbox("Butter Walk (more likely to take SERVER side hits)", &g_cfg.butterWalk);
            ImGui::SliderFloat("Hit Box Size", &g_cfg.dodgeHitboxSize, 0.451f, 0.501f, "%.3f");
            ImGui::SliderInt("Move Away Buffer (ms)", &g_cfg.dodgeMoveAwayMs, 0, 1000);
            ImGui::Checkbox("Dodge AoE/Bombs", &g_cfg.dodgeAoeBombs);
            ImGui::Checkbox("Avoid Units", &g_cfg.dodgeAvoidUnits);
            ImGui::SliderFloat("Unit Avoidance Scale", &g_cfg.dodgeUnitAvoidanceScale, 0.0f, 1.5f, "%.2f");
            ImGui::Checkbox("Old Dodge Logic (don't use this)", &g_cfg.oldDodgeLogic);
        }

        void FollowTab() {
            ImGui::TextUnformatted("Follow Settings");
            ImGui::Separator();
            ImGui::TextDisabled("Follow hooks are not part of the current feature layer.");
            ImGui::Checkbox("Teleport if Out of Range", &g_cfg.teleportIfOutOfRange);
            ImGui::Checkbox("Nexus When Lost", &g_cfg.nexusWhenLost);
        }

        void RenderTab() {
            ImGui::TextUnformatted("Render Options Settings");
            ImGui::Separator();
            if (ImGui::BeginTabBar("##RenderNestedTab")) {
                if (ImGui::BeginTabItem("POI/Bags")) {
                    ImGui::Checkbox("Enable POIs + Bags", &g_cfg.enablePoisBags);
                    ImGui::Checkbox("Play sound.wav For Enabled Bags", &g_cfg.playSoundForBags);
                    ImGui::TextUnformatted("== Bags ==");
                    ImGui::Checkbox("Egg", &g_cfg.bagEgg); ImGui::SameLine();
                    ImGui::Checkbox("Brown", &g_cfg.bagBrown); ImGui::SameLine();
                    ImGui::Checkbox("Pink", &g_cfg.bagPink); ImGui::SameLine();
                    ImGui::Checkbox("Purple", &g_cfg.bagPurple);
                    ImGui::Checkbox("Cyan", &g_cfg.bagCyan); ImGui::SameLine();
                    ImGui::Checkbox("Dark Blue", &g_cfg.bagDarkBlue); ImGui::SameLine();
                    ImGui::Checkbox("White", &g_cfg.bagWhite);
                    ImGui::Checkbox("Gold", &g_cfg.bagGold); ImGui::SameLine();
                    ImGui::Checkbox("Orange", &g_cfg.bagOrange); ImGui::SameLine();
                    ImGui::Checkbox("Red", &g_cfg.bagRed);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Other")) {
                    ImGui::Checkbox("Render Projectiles", &g_cfg.renderProjectiles);
                    ImGui::Checkbox("Render AOE Debug", &g_cfg.renderAoeDebug);
                    ImGui::Checkbox("Render Tiles", &g_cfg.renderTiles);
                    ImGui::Checkbox("Render Units", &g_cfg.renderUnits);
                    ImGui::Checkbox("Render Hitbox", &g_cfg.renderHitbox);
                    ImGui::Checkbox("Render Grid", &g_cfg.renderGrid);
                    ImGui::Checkbox("Render Safety Path", &g_cfg.renderSafetyPath);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Menu Theme")) {
                    ImGui::TextUnformatted("Customize Theme Colors:");
                    ImGui::Checkbox("Menu Background", &g_cfg.menuBackground);
                    ImGui::Checkbox("Title Bar Active", &g_cfg.titleBarActive);
                    ImGui::Checkbox("Side Bar Background", &g_cfg.sideBarBackground);
                    ImGui::ColorEdit4("Base", g_cfg.colorBase);
                    ImGui::ColorEdit4("Hover", g_cfg.colorHover);
                    ImGui::ColorEdit4("Active", g_cfg.colorActive);
                    ImGui::ColorEdit4("Check", g_cfg.colorCheck);
                    ImGui::ColorEdit4("Text Color", g_cfg.colorText);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        void SpooferTab() {
            ImGui::TextUnformatted("Spoofing Settings");
            ImGui::Separator();
            ImGui::TextDisabled("Identity and appearance hooks are not part of the current feature layer.");
            ImGui::Checkbox("Enable Glow", &g_cfg.enableGlow);
            ImGui::RadioButton("Outline", &g_cfg.glowStyle, 0); ImGui::SameLine();
            ImGui::RadioButton("Glow", &g_cfg.glowStyle, 1);
            ImGui::Checkbox("Rainbow Glow", &g_cfg.rainbowGlow);
            ImGui::Checkbox("Fame Value", &g_cfg.fameValue);
            if (g_cfg.fameValue) ImGui::InputFloat("##FameValue", &g_cfg.fameValueAmount);
            ImGui::Checkbox("Account Fame", &g_cfg.accountFame);
            if (g_cfg.accountFame) ImGui::InputFloat("Account Fame Value", &g_cfg.accountFameValue);
        }

        void MiscTab() {
            static Keybind menuBind;
            static int syncedVk = -1;
            if (!menuBind.listening && syncedVk != g_cfg.menuToggleHotkey) {
                menuBind.vk = g_cfg.menuToggleHotkey;
                syncedVk = g_cfg.menuToggleHotkey;
            }

            ImGui::TextUnformatted("Debug / Misc Settings");
            ImGui::Separator();
            ImGui::TextUnformatted("Menu Toggle Key");
            ImGui::SameLine(145.0f);
            HotkeyWidget("menuToggleHotkey", menuBind);
            g_cfg.menuToggleHotkey = menuBind.vk > 0 ? menuBind.vk : 0;
            syncedVk = g_cfg.menuToggleHotkey;
            ImGui::SliderFloat("Doge Teleport Max", &g_cfg.dogeTeleportMax, 0.0f, 1.5f, "%.2f");
            ImGui::TextDisabled("Config: %s", Config_Path());
        }
    }

    void Toggle() {
        const DWORD now = GetTickCount();
        if (now - s_lastToggle < 120) return;
        s_lastToggle = now;
        s_open = !s_open;
    }

    void SetOpen(bool open) { s_open = open; }
    bool IsOpen() { return s_open; }

    void ApplyTheme(int) {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 5.0f);

        ImVec4* c = style.Colors;
        c[ImGuiCol_Text] = Color(g_cfg.colorText);
        c[ImGuiCol_WindowBg] = g_cfg.menuBackground ? Color(g_cfg.colorMenuBackground) : ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ChildBg] = g_cfg.sideBarBackground ? Color(g_cfg.colorSidebar) : ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TitleBg] = Color(g_cfg.colorSidebar);
        c[ImGuiCol_TitleBgActive] = g_cfg.titleBarActive ? Color(g_cfg.colorTitleActive) : Color(g_cfg.colorSidebar);
        c[ImGuiCol_FrameBg] = Color(g_cfg.colorSidebar);
        c[ImGuiCol_FrameBgHovered] = Color(g_cfg.colorHover);
        c[ImGuiCol_FrameBgActive] = Color(g_cfg.colorActive);
        c[ImGuiCol_Button] = Color(g_cfg.colorBase);
        c[ImGuiCol_ButtonHovered] = Color(g_cfg.colorHover);
        c[ImGuiCol_ButtonActive] = Color(g_cfg.colorActive);
        c[ImGuiCol_CheckMark] = Color(g_cfg.colorCheck);
        c[ImGuiCol_SliderGrab] = Color(g_cfg.colorBase);
        c[ImGuiCol_SliderGrabActive] = Color(g_cfg.colorActive);
        c[ImGuiCol_Header] = Color(g_cfg.colorBase);
        c[ImGuiCol_HeaderHovered] = Color(g_cfg.colorHover);
        c[ImGuiCol_HeaderActive] = Color(g_cfg.colorActive);
        c[ImGuiCol_Tab] = Color(g_cfg.colorSidebar);
        c[ImGuiCol_TabHovered] = Color(g_cfg.colorHover);
        c[ImGuiCol_TabSelected] = Color(g_cfg.colorActive);
    }

    void Render() {
        if (!s_open) return;
        ApplyTheme(g_cfg.menuTheme);

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float scale = std::max(0.65f, std::min(display.x / 1920.0f, display.y / 1080.0f));
        ImGui::SetNextWindowSize(ImVec2(1120.0f * scale, 610.0f * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(760.0f * scale, 430.0f * scale), display);

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
        if (!ImGui::Begin("DogeBawt", &s_open, flags)) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Version v1.1.16");
        ImGui::SameLine(ImGui::GetWindowWidth() - 120.0f);
        if (ImGui::Button("Save Config", ImVec2(105.0f, 0.0f))) Config_Save();
        ImGui::Separator();

        if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (ImGui::BeginTabItem("\t\tMods")) { ModsTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("\t\tAuto Nexus")) { NexusTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("\t\tAuto Aim")) { AimTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("\t\tLag Port")) {
                ImGui::TextUnformatted("Lag Port Settings");
                ImGui::Separator();
                ImGui::TextDisabled("Lag-port hooks are not part of the current feature layer.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("\t\tAuto Dodge")) { DodgeTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("\t\tFollow")) { FollowTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("\t\tRender")) { RenderTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("\t\tSpoofer")) { SpooferTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("\t\tDebug / Misc")) { MiscTab(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
}
