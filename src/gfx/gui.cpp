#include "gfx/gui.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/bridge.h"
#include "core/config.h"
#include "core/overlay.h"
#include "gfx/renderer.h"
#include "gfx/theme.h"
#include "gfx/widgets.h"
#include "util/anim.h"

namespace uv::gui {
namespace {

constexpr int kTabCount = 5;
constexpr float kSidebar = 214.f;
constexpr float kHeader = 84.f;
constexpr float kFooter = 54.f;

struct TabInfo {
    const char* nav;
    const char* title;
    const char* subtitle;
};

const TabInfo kTabs[kTabCount] = {
    {"HUD", "HUD Editor", "Arrange your counters, then drag them on screen"},
    {"World Visuals", "World Visuals", "Client-side time, fog, tint and buttery smooth zoom"},
    {"Player Cosmetics", "Player Cosmetics", "China hat, jump rings and a sparkling particle trail"},
    {"Animations", "Animations & Viewmodel", "Keystroke feedback, swing weight and hand placement"},
    {"Config", "Config", "Interface scale, theme, keybinds and the config file"},
};

int g_tab = 0;
int g_lastTab = -1;
float g_navHover[kTabCount] = {};
float g_closeHover = 0.f;
ImVec2 g_pos(0.f, 0.f);
bool g_posInit = false;
float g_scroll[kTabCount] = {};

float A() { return ImGui::GetStyle().Alpha; }

float ActiveCount() {
    float count = 0.f;
    for (int i = 0; i < kHudElementCount; ++i) count += cfg.hud[i].enabled ? 1.f : 0.f;
    count += cfg.timeOn ? 1.f : 0.f;
    count += cfg.fogOn ? 1.f : 0.f;
    count += cfg.tintOn ? 1.f : 0.f;
    count += cfg.zoomOn ? 1.f : 0.f;
    count += cfg.hatOn ? 1.f : 0.f;
    count += cfg.ringsOn ? 1.f : 0.f;
    count += cfg.trailOn ? 1.f : 0.f;
    count += cfg.smoothSwing ? 1.f : 0.f;
    count += cfg.viewmodelOn ? 1.f : 0.f;
    return count;
}

void Text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text) {
    dl->AddText(font, size, pos, color, text);
}

void Icon(ImDrawList* dl, int tab, ImVec2 c, ImU32 color) {
    switch (tab) {
        case 0:  // HUD: four tiles
            for (int i = 0; i < 4; ++i) {
                const float x = c.x - 6.f + (float)(i % 2) * 7.f;
                const float y = c.y - 6.f + (float)(i / 2) * 7.f;
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 5.f, y + 5.f), color, 1.5f);
            }
            break;
        case 1:  // world: globe
            dl->AddCircle(c, 7.f, color, 20, 1.4f);
            dl->AddEllipse(c, ImVec2(3.2f, 7.f), color, 0.f, 20, 1.2f);
            dl->AddLine(ImVec2(c.x - 7.f, c.y), ImVec2(c.x + 7.f, c.y), color, 1.2f);
            break;
        case 2: {  // cosmetics: sparkle
            const float r = 8.f;
            dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + 2.4f, c.y - 2.4f), ImVec2(c.x + r, c.y),
                              ImVec2(c.x + 2.4f, c.y + 2.4f), color);
            dl->AddQuadFilled(ImVec2(c.x, c.y + r), ImVec2(c.x - 2.4f, c.y + 2.4f), ImVec2(c.x - r, c.y),
                              ImVec2(c.x - 2.4f, c.y - 2.4f), color);
            dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x - 2.4f, c.y - 2.4f), ImVec2(c.x - r, c.y),
                              ImVec2(c.x - 2.4f, c.y + 2.4f), color);
            dl->AddQuadFilled(ImVec2(c.x, c.y + r), ImVec2(c.x + 2.4f, c.y + 2.4f), ImVec2(c.x + r, c.y),
                              ImVec2(c.x + 2.4f, c.y - 2.4f), color);
            break;
        }
        case 3: {  // animations: bars
            const float hs[3] = {8.f, 14.f, 5.f};
            for (int i = 0; i < 3; ++i) {
                const float x = c.x - 7.f + (float)i * 6.f;
                dl->AddRectFilled(ImVec2(x, c.y + 7.f - hs[i]), ImVec2(x + 4.f, c.y + 7.f), color, 1.5f);
            }
            break;
        }
        default:  // config: gear-ish
            dl->AddCircle(c, 5.5f, color, 20, 1.5f);
            dl->AddCircleFilled(c, 2.f, color, 12);
            for (int i = 0; i < 6; ++i) {
                const float a = (float)i / 6.f * 6.2831853f;
                dl->AddCircleFilled(ImVec2(c.x + std::cos(a) * 7.6f, c.y + std::sin(a) * 7.6f), 1.15f, color, 6);
            }
            break;
    }
}

void Brand(ImDrawList* dl, ImVec2 p) {
    const ImVec2 mark(p.x, p.y);
    const ImVec2 markEnd(mark.x + 38.f, mark.y + 38.f);
    dl->AddRectFilled(mark, markEnd, Alpha(theme::Acc(1.f), 0.16f * A()), 11.f);
    dl->AddRect(mark, markEnd, Alpha(theme::Acc(1.f), 0.42f * A()), 11.f);
    const ImVec2 textSize = renderer::FontBig()->CalcTextSizeA(14.f, FLT_MAX, 0.f, "UV");
    dl->AddText(renderer::FontBig(), 14.f,
                ImVec2((mark.x + markEnd.x - textSize.x) * 0.5f, (mark.y + markEnd.y - textSize.y) * 0.5f),
                Alpha(theme::Acc(1.f), 1.f), "UV");

    dl->AddText(renderer::FontMedium(), 14.5f, ImVec2(p.x + 48.f, p.y + 2.f),
                IM_COL32(240, 240, 248, (int)(250 * A())), "UniversalVisuals");
    dl->AddText(renderer::FontSmall(), 9.f, ImVec2(p.x + 49.f, p.y + 23.f),
                IM_COL32(140, 140, 158, (int)(230 * A())), "OPENGL OVERLAY");
    const char* version = "v1.0";
    const float vw = renderer::FontSmall()->CalcTextSizeA(9.f, FLT_MAX, 0.f, version).x;
    dl->AddText(renderer::FontSmall(), 9.f, ImVec2(p.x + 186.f - vw, p.y + 23.f),
                Alpha(theme::Acc(1.f), 0.9f), version);
}

bool NavItem(ImDrawList* dl, ImVec2 wp, float y, int index) {
    const float x = wp.x + 13.f;
    const float w = kSidebar - 26.f;
    const float h = 44.f;
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    char id[16];
    std::snprintf(id, sizeof id, "##nav%d", index);
    ImGui::InvisibleButton(id, ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    g_navHover[index] = SmoothTowards(g_navHover[index], hovered ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);
    const bool active = g_tab == index;
    const float hover = g_navHover[index];

    if (active) {
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), Alpha(theme::Acc(1.f), 0.13f * A()), 9.f);
        dl->AddRectFilled(ImVec2(x, y + 10.f), ImVec2(x + 3.f, y + h - 10.f), Alpha(theme::Acc(1.f), A()), 1.5f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), Alpha(theme::Acc(1.f), 0.22f * A()), 9.f);
    } else if (hover > 0.01f) {
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(255, 255, 255, (int)(9 * hover * A())), 9.f);
    }

    const ImU32 iconColor = active ? Alpha(theme::Acc(1.f), 1.f)
                                   : IM_COL32(160, 160, 178, (int)((200 + 40 * hover) * A()));
    Icon(dl, index, ImVec2(x + 22.f, y + h * 0.5f), iconColor);

    dl->AddText(renderer::Font(), 13.5f, ImVec2(x + 40.f, y + 14.f),
                active ? IM_COL32(245, 245, 252, (int)(250 * A())) : IM_COL32(190, 190, 205, (int)((210 + 30 * hover) * A())),
                kTabs[index].nav);

    char number[4];
    std::snprintf(number, sizeof number, "%d", index + 1);
    dl->AddText(renderer::FontSmall(), 9.f, ImVec2(x + w - 16.f, y + 16.f),
                IM_COL32(120, 120, 140, (int)((130 + 60 * hover) * A())), number);
    return clicked;
}

void Pill(ImDrawList* dl, ImVec2 rightTop, const char* text, ImU32 color) {
    const float tw = renderer::FontSmall()->CalcTextSizeA(10.f, FLT_MAX, 0.f, text).x;
    const ImVec2 a(rightTop.x - tw - 26.f, rightTop.y);
    const ImVec2 b(rightTop.x, rightTop.y + 22.f);
    dl->AddRectFilled(a, b, Alpha(color, 0.12f * A()), 11.f);
    dl->AddRect(a, b, Alpha(color, 0.30f * A()), 11.f);
    dl->AddCircleFilled(ImVec2(a.x + 10.f, a.y + 11.f), 3.f, Alpha(color, A()), 12);
    dl->AddText(renderer::FontSmall(), 10.f, ImVec2(a.x + 17.f, a.y + 5.5f), Alpha(color, A()), text);
}

void TimeChips() {
    struct Preset {
        const char* label;
        int value;
    };
    static const Preset presets[] = {
        {"Sunrise", 23000}, {"Noon", 6000}, {"Sunset", 12000}, {"Midnight", 18000},
    };
    const ImVec2 start = ImGui::GetCursorScreenPos();
    float x = start.x;
    for (const Preset& preset : presets) {
        const float w = renderer::FontSmall()->CalcTextSizeA(12.f, FLT_MAX, 0.f, preset.label).x + 26.f;
        ImGui::SetCursorScreenPos(ImVec2(x, start.y));
        if (widgets::Chip(preset.label, cfg.timeValue == preset.value, w)) {
            cfg.timeValue = preset.value;
            cfg.timeOn = true;
        }
        x += w + 7.f;
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + 34.f));
}

void AccentPicker() {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    float x = start.x;
    for (int i = 0; i < theme::PresetCount(); ++i) {
        const ImVec4 color = theme::PresetColor(i);
        const ImVec2 p(x, start.y);
        ImGui::SetCursorScreenPos(p);
        char id[24];
        std::snprintf(id, sizeof id, "##preset%d", i);
        ImGui::InvisibleButton(id, ImVec2(24.f, 24.f));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) {
            theme::ApplyPreset(i);
            overlay::Toast(theme::PresetName(i));
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool active = std::fabs(cfg.accentR - color.x) < 0.01f && std::fabs(cfg.accentG - color.y) < 0.01f &&
                            std::fabs(cfg.accentB - color.z) < 0.01f;
        dl->AddCircleFilled(ImVec2(p.x + 10.f, p.y + 10.f), 8.f, Col(color.x, color.y, color.z, 1.f), 20);
        if (active) {
            dl->AddCircle(ImVec2(p.x + 10.f, p.y + 10.f), 10.5f, IM_COL32(255, 255, 255, (int)(220 * A())), 24, 1.4f);
        } else if (hovered) {
            dl->AddCircle(ImVec2(p.x + 10.f, p.y + 10.f), 10.5f, IM_COL32(255, 255, 255, (int)(90 * A())), 24, 1.f);
        }
        x += 30.f;
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + 32.f));
}

void HudTab() {
    widgets::Section("COUNTERS");
    widgets::Toggle("hud_wm", &cfg.hud[HUD_Watermark].enabled, "Watermark", "Gradient brand chip with a moving highlight");
    widgets::Toggle("hud_fps", &cfg.hud[HUD_Fps].enabled, "FPS counter", "Frame rate plus a live 9 second graph");
    widgets::Toggle("hud_cps", &cfg.hud[HUD_Cps].enabled, "CPS counter", "Left and right clicks per second");
    widgets::Toggle("hud_xyz", &cfg.hud[HUD_Coords].enabled, "Coordinates", "XYZ readout with nether conversion");
    widgets::Toggle("hud_armor", &cfg.hud[HUD_Armor].enabled, "Armor status", "Durability bars for all four slots");
    widgets::Toggle("hud_keys", &cfg.hud[HUD_Keystrokes].enabled, "Keystrokes", "WASD, space and shift with press feedback");

    widgets::Section("APPEARANCE");
    widgets::SliderF("hud_scale", &cfg.hudScale, 0.6f, 1.6f, "HUD scale", "%.2fx");
    widgets::SliderF("hud_spring", &cfg.hudSpring, 0.4f, 2.5f, "Keystroke spring", "%.2fx");
    widgets::Toggle("hud_bg", &cfg.hudBackground, "Glass background", "Rounded panel behind every element");
    widgets::Toggle("hud_shadow", &cfg.hudShadow, "Soft shadows", "Layered drop shadow under each card");
    widgets::Toggle("hud_bars", &cfg.hudBars, "Accent bars", "Thin gradient marker on the left edge");

    widgets::Hint("While this menu is open, drag any HUD element to move it. Positions are stored as fractions of "
                  "the screen, so they survive resolution and fullscreen changes.");
}

void WorldTab() {
    widgets::Section("TIME OF DAY");
    widgets::Toggle("time_on", &cfg.timeOn, "Client-side time override", "Only changes what you see, never the server");
    {
        char clock[32];
        const int hours = ((cfg.timeValue / 1000) + 6) % 24;
        const int minutes = (cfg.timeValue % 1000) * 60 / 1000;
        std::snprintf(clock, sizeof clock, "%02d:%02d  ·  %d ticks", hours, minutes, cfg.timeValue);
        widgets::Hint(clock);
    }
    TimeChips();
    widgets::SliderI("time_value", &cfg.timeValue, 0, 24000, "Time");

    widgets::Section("ATMOSPHERE");
    widgets::Toggle("fog_on", &cfg.fogOn, "Custom fog", "Colour and density override for the distance fog");
    widgets::ColorRow("fog_color", &cfg.fogColor.r, "Fog colour");
    widgets::SliderPercent("fog_density", &cfg.fogDensity, 0.f, 1.f, "Fog density");
    widgets::Toggle("tint_on", &cfg.tintOn, "Screen tint", "Cinematic colour grade with a soft vignette");
    widgets::ColorRow("tint_color", &cfg.tintColor.r, "Tint colour");
    widgets::SliderPercent("tint_strength", &cfg.tintStrength, 0.02f, 0.5f, "Tint strength");

    widgets::Section("CAMERA");
    widgets::Toggle("zoom_on", &cfg.zoomOn, "Smooth zoom", "Hold the zoom key for an OptiFine-style FOV lerp");
    widgets::KeybindRow("zoom_key", &cfg.keyZoom, "Zoom key");
    widgets::SliderPercent("zoom_factor", &cfg.zoomFactor, 0.15f, 0.9f, "Zoomed FOV");
    widgets::SliderF("zoom_smooth", &cfg.zoomSmooth, 2.f, 24.f, "Zoom smoothing", "%.1fx");

    if (!bridge::HasTimeControl())
        widgets::Hint("Time override needs GameBridge::SetWorldTime() wired to your Minecraft version.");
    if (!bridge::HasFogControl())
        widgets::Hint("Fog needs GameBridge::SetFog() wired to your Minecraft version.");
    if (!bridge::HasFovControl())
        widgets::Hint("Zoom needs GameBridge::SetFov() wired to your Minecraft version.");
}

void CosmeticsTab() {
    widgets::Toggle("demo_mode", &cfg.demoCosmetics, "Demo preview",
                    "Synthetic player anchors so you can style everything without launching a world");

    widgets::Section("CHINA HAT");
    widgets::Toggle("hat_on", &cfg.hatOn, "China hat", "Layered 3D cone with a spinning highlight");
    widgets::SliderF("hat_scale", &cfg.hatScale, 0.4f, 2.5f, "Size", "%.2fx");
    widgets::SliderF("hat_tilt", &cfg.hatTilt, -60.f, 60.f, "Tilt", "%.0f°");
    widgets::SliderF("hat_spin", &cfg.hatSpin, 0.f, 180.f, "Spin speed", "%.0f°/s");
    widgets::ColorRow("hat_col_a", &cfg.hatColorA.r, "Primary colour");
    widgets::ColorRow("hat_col_b", &cfg.hatColorB.r, "Highlight colour");

    widgets::Section("JUMP RINGS");
    widgets::Toggle("rings_on", &cfg.ringsOn, "Jump rings", "Expanding glowing rings on every take-off");
    widgets::SliderF("ring_life", &cfg.ringLife, 0.3f, 3.f, "Lifetime", "%.2fs");
    widgets::SliderF("ring_size", &cfg.ringSize, 0.3f, 3.f, "Radius", "%.2fx");
    widgets::ColorRow("ring_color", &cfg.ringColor.r, "Ring colour");

    widgets::Section("PARTICLE TRAIL");
    widgets::Toggle("trail_on", &cfg.trailOn, "Particle trail", "Soft rising sparks around your feet");
    widgets::SliderF("trail_rate", &cfg.trailRate, 2.f, 120.f, "Spawn rate", "%.0f/s");
    widgets::SliderF("trail_life", &cfg.trailLife, 0.3f, 4.f, "Lifetime", "%.2fs");
    widgets::SliderF("trail_size", &cfg.trailSize, 0.3f, 3.f, "Particle size", "%.2fx");
    widgets::ColorRow("trail_color", &cfg.trailColor.r, "Trail colour");

    widgets::Hint("Cosmetics use the screen anchors supplied by GameBridge. Enable Demo preview to see them right "
                  "now — the anchors follow a virtual walk cycle.");
}

void AnimationsTab() {
    widgets::Section("KEYSTROKES");
    widgets::Toggle("key_anim", &cfg.keyAnimations, "Press animations", "Keys scale, glow and lift when pressed");
    widgets::SliderF("key_spring", &cfg.hudSpring, 0.4f, 2.5f, "Spring stiffness", "%.2fx");

    widgets::Section("VIEWMODEL");
    widgets::Toggle("swing_on", &cfg.smoothSwing, "Smooth swing", "Eases the hand swing curve instead of snapping");
    widgets::SliderF("swing_speed", &cfg.swingSpeed, 0.3f, 3.f, "Swing speed", "%.2fx");
    widgets::Toggle("vm_on", &cfg.viewmodelOn, "Hand transform", "Offset, scale and rotate the first-person hand");
    widgets::SliderF("vm_x", &cfg.viewX, -1.f, 1.f, "Offset X", "%.2f");
    widgets::SliderF("vm_y", &cfg.viewY, -1.f, 1.f, "Offset Y", "%.2f");
    widgets::SliderF("vm_z", &cfg.viewZ, -1.f, 1.f, "Offset Z", "%.2f");
    widgets::SliderF("vm_scale", &cfg.viewScale, 0.5f, 2.f, "Hand scale", "%.2fx");
    if (widgets::Button("Reset hand transform", false, 176.f)) {
        cfg.viewX = 0.f;
        cfg.viewY = 0.f;
        cfg.viewZ = 0.f;
        cfg.viewScale = 1.f;
        overlay::Toast("Hand transform reset");
    }

    if (!bridge::HasViewModelControl())
        widgets::Hint("Hand transform needs GameBridge::ApplyViewModel() wired to your Minecraft version.");
    else if (cfg.viewmodelOn)
        widgets::Hint("ApplyViewModel is active.");
}

void ConfigTab() {
    widgets::Section("INTERFACE");
    widgets::SliderF("ui_scale", &cfg.uiScale, 0.75f, 2.f, "UI scale", "%.2fx");
    widgets::SliderPercent("menu_opacity", &cfg.menuOpacity, 0.55f, 1.f, "Menu opacity");
    widgets::SliderF("menu_speed", &cfg.menuAnimSpeed, 4.f, 30.f, "Menu animation", "%.0fx");

    widgets::Section("THEME");
    AccentPicker();
    float accent[3] = {cfg.accentR, cfg.accentG, cfg.accentB};
    if (widgets::ColorRow("accent_color", accent, "Accent colour")) {
        cfg.accentR = accent[0];
        cfg.accentG = accent[1];
        cfg.accentB = accent[2];
        cfg.rainbowAccent = false;
    }
    widgets::Toggle("rainbow", &cfg.rainbowAccent, "Rainbow accent", "Slowly cycles the accent hue");

    widgets::Section("KEYBINDS");
    widgets::KeybindRow("key_menu", &cfg.keyMenu, "Open menu");
    widgets::KeybindRow("key_toggle", &cfg.keyToggle, "Toggle everything");
    widgets::KeybindRow("key_zoom", &cfg.keyZoom, "Smooth zoom");

    widgets::Section("BEHAVIOUR");
    widgets::Toggle("overlay_on", &cfg.overlayEnabled, "Visuals enabled", "Master switch, also toggled with the keybind");
    widgets::Toggle("hide_outside", &cfg.hideOutsideGame, "Hide outside a world",
                    "Draw nothing on the title screen, pause menu or server list");
    widgets::Toggle("autosave", &cfg.autoSave, "Auto-save", "Write the config whenever the menu closes");

    widgets::Section("CONFIG FILE");
    widgets::Hint(ConfigPathA());
    if (widgets::Button("Save now", false, 108.f)) {
        SaveConfig();
        overlay::Toast("Config saved");
    }
    ImGui::SameLine(0.f, 8.f);
    if (widgets::Button("Restore defaults", false, 148.f)) ImGui::OpenPopup("##restore");
    if (ImGui::BeginPopup("##restore")) {
        ImGui::TextUnformatted("Reset every setting to its default?");
        ImGui::Spacing();
        if (ImGui::Button("Yes, reset", ImVec2(110.f, 28.f))) {
            ResetConfig();
            SaveConfig();
            overlay::Toast("Settings restored");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.f, 28.f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void DrawTabContent() {
    switch (g_tab) {
        case 0: HudTab(); break;
        case 1: WorldTab(); break;
        case 2: CosmeticsTab(); break;
        case 3: AnimationsTab(); break;
        default: ConfigTab(); break;
    }
}

}  // namespace

void SetTab(int index) {
    if (index < 0) index = 0;
    if (index >= kTabCount) index = kTabCount - 1;
    g_tab = index;
}

int Tab() { return g_tab; }

void Draw(bool open, float anim) {
    if (anim <= 0.002f) return;

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = renderer::DisplaySize();
    const ImVec2 size(std::max(520.f, std::min(872.f, display.x - 24.f)),
                      std::max(430.f, std::min(612.f, display.y - 24.f)));

    if (!g_posInit) {
        g_pos = ImVec2((display.x - size.x) * 0.5f, (display.y - size.y) * 0.5f);
        g_posInit = true;
    }
    ImVec2 target = g_pos;
    target.x = Clamp(target.x, 6.f, std::max(6.f, display.x - size.x - 6.f));
    target.y = Clamp(target.y, 6.f, std::max(6.f, display.y - size.y - 6.f));

    const float t = EaseOutQuint(anim);
    const ImVec2 center((display.x - size.x) * 0.5f, (display.y - size.y) * 0.5f);
    const ImVec2 pos(Lerp(center.x, target.x, t), Lerp(center.y + 14.f, target.y, t));

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, EaseOutCubic(anim));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::Begin("##uvmenu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 wb(wp.x + size.x, wp.y + size.y);

    // ------------------------------------------------------------------ shell
    for (int i = 7; i >= 1; --i) {
        const float grow = (float)i * 6.f;
        dl->AddRectFilled(ImVec2(wp.x - grow, wp.y - grow + 10.f), ImVec2(wb.x + grow, wb.y + grow + 10.f),
                          IM_COL32(0, 0, 0, (int)(7 * A())), 18.f + grow);
    }
    const int bgA = (int)(255 * cfg.menuOpacity * A());
    dl->AddRectFilled(wp, wb, IM_COL32(17, 17, 25, bgA), 15.f);
    dl->AddRectFilledMultiColor(ImVec2(wp.x + 1.f, wp.y + 1.f), ImVec2(wb.x - 1.f, wp.y + 130.f),
                                IM_COL32(26, 24, 40, (int)(140 * A())), IM_COL32(20, 20, 30, (int)(60 * A())),
                                IM_COL32(20, 20, 30, 0), IM_COL32(20, 20, 30, 0));
    dl->AddRect(wp, wb, IM_COL32(255, 255, 255, (int)(24 * A())), 15.f, 0, 1.f);
    dl->AddLine(ImVec2(wp.x + 15.f, wp.y + 1.f), ImVec2(wb.x - 15.f, wp.y + 1.f),
                IM_COL32(255, 255, 255, (int)(18 * A())));

    // Ambient accent glows inside the panel.
    dl->PushClipRect(wp, wb, true);
    dl->AddCircleFilled(ImVec2(wp.x + 60.f, wp.y + 24.f), 190.f, Alpha(theme::Acc(1.f), 0.045f * A()), 56);
    dl->AddCircleFilled(ImVec2(wb.x - 40.f, wb.y - 30.f), 210.f, Alpha(theme::AccAlt(1.f), 0.030f * A()), 56);
    dl->PopClipRect();

    dl->AddLine(ImVec2(wp.x + kSidebar, wp.y + 16.f), ImVec2(wp.x + kSidebar, wb.y - 16.f),
                IM_COL32(255, 255, 255, (int)(16 * A())));

    // ----------------------------------------------------------------- window
    Brand(dl, ImVec2(wp.x + 17.f, wp.y + 22.f));

    float navY = wp.y + 96.f;
    for (int i = 0; i < kTabCount; ++i) {
        if (NavItem(dl, wp, navY, i)) SetTab(i);
        navY += 47.f;
    }

    // Sidebar status card.
    {
        const ImVec2 a(wp.x + 13.f, wb.y - 108.f);
        const ImVec2 b(wp.x + kSidebar - 13.f, wb.y - 38.f);
        dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, (int)(7 * A())), 10.f);
        dl->AddRect(a, b, IM_COL32(255, 255, 255, (int)(18 * A())), 10.f);
        dl->AddText(renderer::FontSmall(), 9.f, ImVec2(a.x + 12.f, a.y + 9.f), IM_COL32(130, 130, 150, (int)(220 * A())),
                    "STATUS");

        const bool inGame = overlay::InGame();
        const bool demo = bridge::Demo();
        const ImU32 dot = inGame ? IM_COL32(96, 214, 140, 255)
                                 : (demo ? IM_COL32(245, 190, 90, 255) : IM_COL32(230, 110, 120, 255));
        const char* state = inGame ? (demo ? "Demo preview" : "In world") : "No world";
        dl->AddCircleFilled(ImVec2(a.x + 15.f, a.y + 31.f), 3.5f, Alpha(dot, A()), 12);
        dl->AddText(renderer::Font(), 13.f, ImVec2(a.x + 25.f, a.y + 23.f), IM_COL32(235, 235, 244, (int)(240 * A())),
                    state);
        dl->AddText(renderer::FontSmall(), 10.f, ImVec2(a.x + 12.f, a.y + 48.f), IM_COL32(140, 140, 158, (int)(225 * A())),
                    cfg.overlayEnabled ? "Visuals enabled" : "Visuals disabled");
        char fps[40];
        std::snprintf(fps, sizeof fps, "%d fps", (int)std::lround(overlay::Fps()));
        const float fw = renderer::FontSmall()->CalcTextSizeA(10.f, FLT_MAX, 0.f, fps).x;
        dl->AddText(renderer::FontSmall(), 10.f, ImVec2(a.x + (b.x - a.x) - 12.f - fw, a.y + 48.f),
                    Alpha(theme::Acc(1.f), 0.9f), fps);
    }

    dl->AddText(renderer::FontSmall(), 9.f, ImVec2(wp.x + 17.f, wb.y - 24.f),
                IM_COL32(118, 118, 138, (int)(200 * A())), "RShift — menu   ·   Esc — close");

    // ----------------------------------------------------------------- header
    ImGui::SetCursorScreenPos(ImVec2(wp.x + kSidebar + 1.f, wp.y));
    ImGui::InvisibleButton("##headerdrag", ImVec2(size.x - kSidebar - 70.f, 74.f));
    if (ImGui::IsItemActive() && anim > 0.85f) {
        g_pos.x += io.MouseDelta.x;
        g_pos.y += io.MouseDelta.y;
        g_pos.x = Clamp(g_pos.x, 6.f, std::max(6.f, display.x - size.x - 6.f));
        g_pos.y = Clamp(g_pos.y, 6.f, std::max(6.f, display.y - size.y - 6.f));
    }

    {
        const float hx = wp.x + kSidebar + 26.f;
        renderer::GradientText(dl, renderer::FontBig(), 21.f, ImVec2(hx, wp.y + 19.f), kTabs[g_tab].title,
                               IM_COL32(250, 250, 255, (int)(255 * A())), Alpha(theme::Acc(1.f), A()));
        dl->AddText(renderer::FontSmall(), 11.f, ImVec2(hx + 1.f, wp.y + 47.f),
                    IM_COL32(150, 150, 168, (int)(230 * A())), kTabs[g_tab].subtitle);
        dl->AddRectFilled(ImVec2(hx, wp.y + 70.f), ImVec2(hx + 52.f, wp.y + 72.f), Alpha(theme::Acc(1.f), A()), 1.f);
        dl->AddRectFilled(ImVec2(hx + 52.f, wp.y + 70.f), ImVec2(hx + 118.f, wp.y + 72.f),
                          Alpha(theme::Acc(1.f), 0.18f * A()), 1.f);
    }

    {
        const bool inGame = overlay::InGame();
        const bool demo = bridge::Demo();
        const char* state = inGame ? (demo ? "DEMO" : "IN GAME") : "NO WORLD";
        const ImU32 color = inGame ? (demo ? IM_COL32(245, 190, 90, 255) : IM_COL32(96, 214, 140, 255))
                                   : IM_COL32(150, 150, 168, 255);
        Pill(dl, ImVec2(wb.x - 66.f, wp.y + 24.f), state, color);
    }

    {
        const ImVec2 a(wb.x - 44.f, wp.y + 20.f);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton("##close", ImVec2(26.f, 26.f));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) overlay::SetMenuOpen(false);
        g_closeHover = SmoothTowards(g_closeHover, hovered ? 1.f : 0.f, 18.f, io.DeltaTime);
        dl->AddRectFilled(a, ImVec2(a.x + 26.f, a.y + 26.f),
                          IM_COL32(235, 90, 110, (int)((24.f + 90.f * g_closeHover) * A())), 7.f);
        const ImU32 cross = IM_COL32(230, 230, 240, (int)((190 + 50 * g_closeHover) * A()));
        dl->AddLine(ImVec2(a.x + 8.f, a.y + 8.f), ImVec2(a.x + 18.f, a.y + 18.f), cross, 1.4f);
        dl->AddLine(ImVec2(a.x + 18.f, a.y + 8.f), ImVec2(a.x + 8.f, a.y + 18.f), cross, 1.4f);
    }

    dl->AddLine(ImVec2(wp.x + kSidebar + 1.f, wp.y + kHeader), ImVec2(wb.x, wp.y + kHeader),
                IM_COL32(255, 255, 255, (int)(16 * A())));

    // ----------------------------------------------------------------- content
    const ImVec2 contentPos(wp.x + kSidebar + 1.f, wp.y + kHeader);
    const ImVec2 contentSize(size.x - kSidebar - 1.f, size.y - kHeader - kFooter);
    ImGui::SetCursorScreenPos(contentPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.f, 12.f));
    ImGui::BeginChild("##content", contentSize, false,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    if (g_lastTab != g_tab) {
        ImGui::SetScrollY(g_scroll[g_tab]);
        g_lastTab = g_tab;
    }
    DrawTabContent();

    // Custom slim scrollbar, drawn inside the child so it scrolls with nothing.
    {
        const float maxScroll = ImGui::GetScrollMaxY();
        const ImVec2 cpos = ImGui::GetWindowPos();
        const float cw = ImGui::GetWindowWidth();
        const float ch = ImGui::GetWindowHeight();
        const float trackTop = cpos.y + 8.f;
        const float trackH = ch - 16.f;
        if (maxScroll > 1.f && trackH > 20.f) {
            const float thumbH = std::max(26.f, trackH * trackH / (trackH + maxScroll));
            const float tScroll = ImGui::GetScrollY() / maxScroll;
            const float y0 = trackTop + (trackH - thumbH) * Clamp01(tScroll);
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            cdl->AddRectFilled(ImVec2(cpos.x + cw - 8.f, trackTop), ImVec2(cpos.x + cw - 5.f, trackTop + trackH),
                               IM_COL32(255, 255, 255, (int)(10 * A())), 2.f);
            cdl->AddRectFilled(ImVec2(cpos.x + cw - 8.f, y0), ImVec2(cpos.x + cw - 5.f, y0 + thumbH),
                               Alpha(theme::Acc(1.f), 0.85f * A()), 2.f);
        }
    }
    g_scroll[g_tab] = ImGui::GetScrollY();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Fades at the top and bottom of the scroll area.
    {
        const int fade = (int)(210 * A());
        dl->AddRectFilledMultiColor(ImVec2(contentPos.x, contentPos.y), ImVec2(wb.x, contentPos.y + 16.f),
                                    IM_COL32(17, 17, 25, fade), IM_COL32(17, 17, 25, fade),
                                    IM_COL32(17, 17, 25, 0), IM_COL32(17, 17, 25, 0));
        dl->AddRectFilledMultiColor(ImVec2(contentPos.x, contentPos.y + contentSize.y - 16.f),
                                    ImVec2(wb.x, contentPos.y + contentSize.y), IM_COL32(17, 17, 25, 0),
                                    IM_COL32(17, 17, 25, 0), IM_COL32(17, 17, 25, fade),
                                    IM_COL32(17, 17, 25, fade));
    }

    // ------------------------------------------------------------------ footer
    const float footerY = wb.y - kFooter;
    dl->AddLine(ImVec2(wp.x + kSidebar + 1.f, footerY), ImVec2(wb.x, footerY),
                IM_COL32(255, 255, 255, (int)(16 * A())));

    {
        char status[128];
        std::snprintf(status, sizeof status, "%d features enabled  ·  %s", (int)std::lround(ActiveCount()),
                      cfg.autoSave ? "auto-save on" : "manual save");
        dl->AddText(renderer::FontSmall(), 10.5f, ImVec2(wp.x + kSidebar + 26.f, footerY + 21.f),
                    IM_COL32(140, 140, 158, (int)(225 * A())), status);
    }

    const float buttonsY = footerY + 10.f;
    ImGui::SetCursorScreenPos(ImVec2(wb.x - 26.f - 92.f - 86.f, buttonsY));
    if (widgets::Button("Save", false, 86.f)) {
        SaveConfig();
        overlay::Toast("Config saved");
    }
    ImGui::SetCursorScreenPos(ImVec2(wb.x - 26.f - 92.f, buttonsY));
    if (widgets::Button("Done", true, 92.f)) overlay::SetMenuOpen(false);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

}  // namespace uv::gui
