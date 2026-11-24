#include "gfx/theme.h"

#include <cmath>

#include "core/config.h"
#include "util/anim.h"

namespace uv::theme {
namespace {

struct Preset {
    const char* name;
    ImVec4 color;
};

const Preset kPresets[] = {
    {"Violet", ImVec4(0.49f, 0.36f, 1.00f, 1.f)},
    {"Aqua", ImVec4(0.13f, 0.83f, 0.93f, 1.f)},
    {"Sunset", ImVec4(1.00f, 0.48f, 0.35f, 1.f)},
    {"Emerald", ImVec4(0.20f, 0.83f, 0.60f, 1.f)},
    {"Rose", ImVec4(0.98f, 0.44f, 0.52f, 1.f)},
    {"Gold", ImVec4(0.98f, 0.75f, 0.30f, 1.f)},
};
constexpr int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

ImVec4 g_accent = kPresets[0].color;
float g_hue = 0.f;

}  // namespace

void Update(float dt) {
    ImVec4 target(g_cfg.accentR, g_cfg.accentG, g_cfg.accentB, 1.f);
    if (g_cfg.rainbowAccent) {
        g_hue = std::fmod(g_hue + dt * 0.055f, 1.f);
        target = Hsv(g_hue, 0.55f, 1.f, 1.f);
    } else {
        g_hue = HueOf(target);
    }
    g_accent.x = SmoothTowards(g_accent.x, target.x, 12.f, dt);
    g_accent.y = SmoothTowards(g_accent.y, target.y, 12.f, dt);
    g_accent.z = SmoothTowards(g_accent.z, target.z, 12.f, dt);
    g_accent.w = 1.f;
}

ImVec4 Accent() { return g_accent; }

ImU32 Acc(float alpha) { return Col(g_accent.x, g_accent.y, g_accent.z, alpha); }

ImU32 AccAlt(float alpha) {
    const ImVec4 alt = Hsv(g_hue + 0.10f, 0.62f, 1.f, 1.f);
    return Col(alt.x, alt.y, alt.z, alpha);
}

ImU32 Grad(float t, float alpha) {
    return Mix(Acc(alpha), AccAlt(alpha), Clamp01(t));
}

ImU32 Wc(ImU32 color) { return ImGui::GetColorU32(color); }

int PresetCount() { return kPresetCount; }
const char* PresetName(int index) {
    return (index >= 0 && index < kPresetCount) ? kPresets[index].name : "?";
}
ImVec4 PresetColor(int index) {
    return (index >= 0 && index < kPresetCount) ? kPresets[index].color : ImVec4(1, 1, 1, 1);
}
void ApplyPreset(int index) {
    if (index < 0 || index >= kPresetCount) return;
    const ImVec4 c = kPresets[index].color;
    g_cfg.accentR = c.x;
    g_cfg.accentG = c.y;
    g_cfg.accentB = c.z;
    g_cfg.rainbowAccent = false;
}

}  // namespace uv::theme
