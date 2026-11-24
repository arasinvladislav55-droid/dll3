#include "gfx/hud.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/bridge.h"
#include "core/config.h"
#include "core/overlay.h"
#include "gfx/renderer.h"
#include "gfx/theme.h"
#include "util/anim.h"

namespace uv::hud {
namespace {

float g_keyAnim[8] = {};
float g_hoverAnim[kHudElementCount] = {};

ImU32 White(float alpha) { return IM_COL32(238, 238, 246, (int)(Clamp01(alpha) * 255.f)); }
ImU32 Dim(float alpha) { return IM_COL32(148, 148, 168, (int)(Clamp01(alpha) * 255.f)); }

void Card(ImDrawList* dl, ImVec2 p, ImVec2 size) {
    const float s = cfg.hudScale;
    const float rounding = 10.f * s;
    const ImVec2 b(p.x + size.x, p.y + size.y);
    if (cfg.hudShadow) {
        dl->AddRectFilled(ImVec2(p.x + 3.f, p.y + 6.f), ImVec2(b.x + 3.f, b.y + 8.f),
                          IM_COL32(0, 0, 0, 55), rounding);
        dl->AddRectFilled(ImVec2(p.x + 1.f, p.y + 3.f), ImVec2(b.x + 1.f, b.y + 4.f),
                          IM_COL32(0, 0, 0, 40), rounding);
    }
    if (cfg.hudBackground) {
        dl->AddRectFilled(p, b, IM_COL32(13, 13, 20, 190), rounding);
        dl->AddRect(p, b, IM_COL32(255, 255, 255, 20), rounding, 0, 1.f);
        dl->AddLine(ImVec2(p.x + rounding, p.y + 1.f), ImVec2(b.x - rounding, p.y + 1.f),
                    IM_COL32(255, 255, 255, 14));
    }
    if (cfg.hudBars) {
        dl->AddRectFilled(ImVec2(p.x, p.y + 9.f), ImVec2(p.x + 2.f, b.y - 9.f), theme::Acc(1.f), 1.f);
    }
}

// Draws text, trimming with an ellipsis when it does not fit `maxWidth`.
void TextClipped(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, float maxWidth, ImU32 color,
                 const char* text) {
    if (font->CalcTextSizeA(size, FLT_MAX, 0.f, text).x <= maxWidth) {
        dl->AddText(font, size, pos, color, text);
        return;
    }
    char buffer[128];
    std::snprintf(buffer, sizeof buffer, "%s", text);
    size_t length = std::strlen(buffer);
    while (length > 4) {
        buffer[--length] = 0;
        char probe[132];
        std::snprintf(probe, sizeof probe, "%s...", buffer);
        if (font->CalcTextSizeA(size, FLT_MAX, 0.f, probe).x <= maxWidth) {
            dl->AddText(font, size, pos, color, probe);
            return;
        }
    }
}

bool BeginElement(const char* id, int index, ImVec2 logicalSize, bool interactive, ImVec2& pos,
                  ImVec2& size) {
    const ImVec2 display = renderer::DisplaySize();
    const float s = cfg.hudScale;
    size = ImVec2(logicalSize.x * s, logicalSize.y * s);
    pos = ImVec2(cfg.hud[index].x * display.x, cfg.hud[index].y * display.y);
    pos.x = Clamp(pos.x, 0.f, std::max(0.f, display.x - size.x));
    pos.y = Clamp(pos.y, 0.f, std::max(0.f, display.y - size.y));

    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(id, size);
    const bool hovered = interactive && ImGui::IsItemHovered();
    if (interactive) {
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && display.x > 1.f && display.y > 1.f) {
            cfg.hud[index].x += ImGui::GetIO().MouseDelta.x / display.x;
            cfg.hud[index].y += ImGui::GetIO().MouseDelta.y / display.y;
            cfg.hud[index].x = Clamp(cfg.hud[index].x, 0.f, 1.f);
            cfg.hud[index].y = Clamp(cfg.hud[index].y, 0.f, 1.f);
        }
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    g_hoverAnim[index] = SmoothTowards(g_hoverAnim[index], hovered ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);

    if (interactive && g_hoverAnim[index] > 0.01f) {
        const float a = g_hoverAnim[index];
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(ImVec2(pos.x - 1.f, pos.y - 1.f), ImVec2(pos.x + size.x + 1.f, pos.y + size.y + 1.f),
                    Alpha(theme::Acc(1.f), 0.75f * a), 11.f * s, 0, 1.2f);
        const float dot = 2.f * s;
        dl->AddCircleFilled(ImVec2(pos.x, pos.y), dot, Alpha(theme::Acc(1.f), a), 8);
        dl->AddCircleFilled(ImVec2(pos.x + size.x, pos.y + size.y), dot, Alpha(theme::Acc(1.f), a), 8);
    }
    return hovered;
}

void Watermark(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = cfg.hudScale;
    Card(dl, pos, size);

    const char* a = "UNIVERSAL";
    const char* b = "VISUALS";
    const float x = pos.x + 12.f * s;
    const float y = pos.y + 9.f * s;
    renderer::GradientText(dl, renderer::FontBig(), 17.f * s, ImVec2(x, y), a, theme::Acc(1.f),
                           theme::AccAlt(1.f));
    const float aw = renderer::FontBig()->CalcTextSizeA(17.f * s, FLT_MAX, 0.f, a).x;
    dl->AddText(renderer::FontBig(), 17.f * s, ImVec2(x + aw + 5.f * s, y), White(0.98f), b);

    dl->AddText(renderer::FontSmall(), 9.5f * s, ImVec2(x, y + 22.f * s), Dim(0.85f), "COSMETIC OVERLAY");

    const char* version = "v1.0";
    const float vw = renderer::FontSmall()->CalcTextSizeA(10.f * s, FLT_MAX, 0.f, version).x;
    const ImVec2 chip(pos.x + size.x - vw - 20.f * s, pos.y + 9.f * s);
    dl->AddRectFilled(chip, ImVec2(chip.x + vw + 10.f * s, chip.y + 15.f * s), Alpha(theme::Acc(1.f), 0.18f), 5.f * s);
    dl->AddText(renderer::FontSmall(), 10.f * s, ImVec2(chip.x + 5.f * s, chip.y + 3.f * s),
                Alpha(theme::Acc(1.f), 1.f), version);

    const float ghost = (std::sin((float)ImGui::GetTime() * 1.6f) * 0.5f + 0.5f) * 0.5f + 0.25f;
    dl->AddLine(ImVec2(x, pos.y + size.y - 6.f * s), ImVec2(pos.x + size.x - 12.f * s, pos.y + size.y - 6.f * s),
                IM_COL32(255, 255, 255, (int)(10 * ghost)), 1.f);
}

void Fps(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = cfg.hudScale;
    Card(dl, pos, size);

    char value[16];
    std::snprintf(value, sizeof value, "%d", (int)std::lround(overlay::Fps()));
    dl->AddText(renderer::FontBig(), 24.f * s, ImVec2(pos.x + 11.f * s, pos.y + 6.f * s), White(0.98f), value);
    const float vw = renderer::FontBig()->CalcTextSizeA(24.f * s, FLT_MAX, 0.f, value).x;
    dl->AddText(renderer::FontSmall(), 10.5f * s, ImVec2(pos.x + 13.f * s + vw, pos.y + 17.f * s),
                Alpha(theme::Acc(1.f), 1.f), "FPS");

    int count = 0;
    const float* history = overlay::FpsHistory(count);
    const float graphH = 13.f * s;
    const float graphW = size.x - 22.f * s;
    const float graphX = pos.x + 11.f * s;
    const float graphY = pos.y + size.y - graphH - 8.f * s;
    const int bars = std::min(count, (int)(graphW / (2.f * s)));

    dl->AddRectFilled(ImVec2(graphX, graphY), ImVec2(graphX + graphW, graphY + graphH),
                      IM_COL32(255, 255, 255, 8), 2.f);
    for (int i = 0; i < bars; ++i) {
        const float v = history[(count - bars + i) % 90];
        const float t = Clamp01(v / 240.f);
        const float h = std::max(1.5f * s, (graphH - 1.f) * t);
        const float bx = graphX + (float)i * 2.f * s;
        dl->AddRectFilled(ImVec2(bx, graphY + graphH - h), ImVec2(bx + 1.4f * s, graphY + graphH),
                          Alpha(theme::Grad((float)i / (float)std::max(1, bars - 1), 1.f), 0.85f), 1.f);
    }
}

void Cps(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = cfg.hudScale;
    Card(dl, pos, size);

    const float halfW = size.x * 0.5f;
    char l[8], r[8];
    std::snprintf(l, sizeof l, "%d", (int)overlay::Cps(true));
    std::snprintf(r, sizeof r, "%d", (int)overlay::Cps(false));

    dl->AddText(renderer::FontBig(), 20.f * s, ImVec2(pos.x + 11.f * s, pos.y + 7.f * s), White(0.98f), l);
    dl->AddText(renderer::FontBig(), 20.f * s, ImVec2(pos.x + halfW + 6.f * s, pos.y + 7.f * s), White(0.98f), r);
    dl->AddText(renderer::FontSmall(), 9.f * s, ImVec2(pos.x + 12.f * s, pos.y + 28.f * s), Dim(0.9f), "LMB");
    dl->AddText(renderer::FontSmall(), 9.f * s, ImVec2(pos.x + halfW + 7.f * s, pos.y + 28.f * s), Dim(0.9f), "RMB");
    dl->AddLine(ImVec2(pos.x + halfW, pos.y + 12.f * s), ImVec2(pos.x + halfW, pos.y + size.y - 12.f * s),
                IM_COL32(255, 255, 255, 14), 1.f);
}

void Coords(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = cfg.hudScale;
    Card(dl, pos, size);

    const bridge::PlayerState& player = bridge::GetPlayer();
    const bool valid = player.valid;
    const float rows[3] = {player.x, player.y, player.z};
    const char axis[3] = {'X', 'Y', 'Z'};
    const ImU32 axisColor[3] = {IM_COL32(255, 96, 118, 255), IM_COL32(120, 220, 150, 255),
                                IM_COL32(110, 160, 255, 255)};

    for (int i = 0; i < 3; ++i) {
        const float y = pos.y + 8.f * s + (float)i * 16.f * s;
        const ImVec2 chip(pos.x + 11.f * s, y);
        dl->AddRectFilled(chip, ImVec2(chip.x + 13.f * s, chip.y + 13.f * s), Alpha(axisColor[i], 0.16f), 4.f);
        char label[2] = {axis[i], 0};
        dl->AddText(renderer::FontSmall(), 10.f * s, ImVec2(chip.x + 4.f * s, chip.y + 2.f * s), axisColor[i], label);
        char value[32];
        if (valid) {
            std::snprintf(value, sizeof value, "%.1f", rows[i]);
        } else {
            std::snprintf(value, sizeof value, "--");
        }
        dl->AddText(renderer::FontMedium(), 12.5f * s, ImVec2(pos.x + 32.f * s, y + 1.f * s), White(0.92f), value);
    }

    if (valid) {
        char nether[40];
        std::snprintf(nether, sizeof nether, "N %.0f %.0f", player.x / 8.f, player.z / 8.f);
        dl->AddText(renderer::FontSmall(), 9.f * s, ImVec2(pos.x + size.x - 70.f * s, pos.y + size.y - 14.f * s),
                    Dim(0.75f), nether);
    } else {
        dl->AddText(renderer::FontSmall(), 9.f * s, ImVec2(pos.x + size.x - 72.f * s, pos.y + size.y - 14.f * s),
                    Dim(0.7f), "no game data");
    }
}

void Armor(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = cfg.hudScale;
    Card(dl, pos, size);

    const bridge::PlayerState& player = bridge::GetPlayer();
    int present = 0;
    for (int i = 0; i < 4; ++i) present += player.armor[i].present ? 1 : 0;
    if (present == 0) {
        dl->AddText(renderer::FontSmall(), 10.5f * s, ImVec2(pos.x + 12.f * s, pos.y + 9.f * s), Dim(0.9f), "ARMOR");
        dl->AddText(renderer::FontSmall(), 9.5f * s, ImVec2(pos.x + 12.f * s, pos.y + 25.f * s), Dim(0.6f),
                    "no armor data");
        return;
    }

    dl->AddText(renderer::FontSmall(), 9.5f * s, ImVec2(pos.x + 12.f * s, pos.y + 7.f * s), Dim(0.8f), "ARMOR");
    int row = 0;
    for (int i = 0; i < 4; ++i) {
        const bridge::ArmorSlot& slot = player.armor[i];
        if (!slot.present) continue;
        const float y = pos.y + 21.f * s + (float)row * 15.f * s;
        ++row;

        TextClipped(dl, renderer::FontSmall(), 10.f * s, ImVec2(pos.x + 12.f * s, y - 1.f * s), 118.f * s,
                    White(0.88f), slot.name);
        const ImVec2 bar(pos.x + size.x - 58.f * s, y + 2.f * s);
        dl->AddRectFilled(bar, ImVec2(bar.x + 44.f * s, bar.y + 3.5f * s), IM_COL32(255, 255, 255, 18), 2.f);
        dl->AddRectFilled(bar, ImVec2(bar.x + 44.f * s * Clamp01(slot.durability), bar.y + 3.5f * s),
                          Alpha(theme::Grad(1.f - slot.durability, 1.f), 1.f), 2.f);

        char pct[8];
        std::snprintf(pct, sizeof pct, "%d", (int)std::lround(slot.durability * 100.f));
        dl->AddText(renderer::FontSmall(), 8.5f * s, ImVec2(pos.x + size.x - 12.f * s -
                                                                renderer::FontSmall()->CalcTextSizeA(8.5f * s, FLT_MAX, 0.f, pct).x,
                                                            y - 1.f * s),
                    Dim(0.8f), pct);
    }
}

void KeyBox(ImDrawList* dl, ImVec2 p, ImVec2 size, const char* label, float anim) {
    const float s = cfg.hudScale;
    const float grow = anim * 1.5f * s;
    const ImVec2 a = ImVec2(p.x + grow, p.y + grow);
    const ImVec2 b = ImVec2(p.x + size.x - grow, p.y + size.y - grow);
    const float rounding = 6.f * s;

    dl->AddRectFilled(a, b, Alpha(theme::Acc(1.f), 0.06f + 0.34f * anim), rounding);
    if (anim > 0.01f) {
        dl->AddRectFilled(ImVec2(a.x, b.y - 2.f * s), ImVec2(b.x, b.y), Alpha(theme::Acc(1.f), anim), rounding,
                          ImDrawFlags_RoundCornersBottom);
    }
    dl->AddRect(a, b, anim > 0.01f ? Alpha(theme::Acc(1.f), 0.35f + 0.45f * anim)
                                   : IM_COL32(255, 255, 255, 22),
                rounding, 0, 1.f);

    const ImVec2 textSize = renderer::FontSmall()->CalcTextSizeA(10.5f * s, FLT_MAX, 0.f, label);
    dl->AddText(renderer::FontSmall(), 10.5f * s,
                ImVec2((a.x + b.x - textSize.x) * 0.5f, (a.y + b.y - textSize.y) * 0.5f + 0.5f * s),
                anim > 0.25f ? White(1.f) : Dim(0.95f), label);
}

void Keystrokes(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = cfg.hudScale;
    Card(dl, pos, size);

    struct Key {
        int vk;
        const char* label;
    };
    const Key keys[6] = {
        {'W', "W"}, {'A', "A"}, {'S', "S"}, {'D', "D"}, {VK_SPACE, "SPACE"}, {VK_LSHIFT, "SHIFT"},
    };
    for (int i = 0; i < 6; ++i) {
        const float target = (GetAsyncKeyState(keys[i].vk) & 0x8000) ? 1.f : 0.f;
        g_keyAnim[i] = SmoothTowards(g_keyAnim[i], target, 22.f * cfg.hudSpring, ImGui::GetIO().DeltaTime);
    }

    const float pad = 9.f * s;
    const float gap = 4.f * s;
    const float keyH = 24.f * s;
    const float keyW = (size.x - pad * 2.f - gap * 2.f) / 3.f;
    const float x0 = pos.x + pad;
    const float y0 = pos.y + pad;

    KeyBox(dl, ImVec2(x0 + keyW + gap, y0), ImVec2(keyW, keyH), "W", g_keyAnim[0]);
    KeyBox(dl, ImVec2(x0, y0 + keyH + gap), ImVec2(keyW, keyH), "A", g_keyAnim[1]);
    KeyBox(dl, ImVec2(x0 + keyW + gap, y0 + keyH + gap), ImVec2(keyW, keyH), "S", g_keyAnim[2]);
    KeyBox(dl, ImVec2(x0 + (keyW + gap) * 2.f, y0 + keyH + gap), ImVec2(keyW, keyH), "D", g_keyAnim[3]);

    const float wideW = size.x - pad * 2.f;
    const float wideY = y0 + (keyH + gap) * 2.f;
    KeyBox(dl, ImVec2(x0, wideY), ImVec2(wideW, 20.f * s), "SPACE", g_keyAnim[4]);
    KeyBox(dl, ImVec2(x0, wideY + 20.f * s + gap), ImVec2(wideW, 16.f * s), "SHIFT", g_keyAnim[5]);
}

}  // namespace

void Draw(bool showWorld, bool menuOpen, float menuAnim) {
    const bool elements = showWorld && cfg.overlayEnabled &&
                          (cfg.hud[HUD_Watermark].enabled || cfg.hud[HUD_Fps].enabled || cfg.hud[HUD_Cps].enabled ||
                           cfg.hud[HUD_Coords].enabled || cfg.hud[HUD_Armor].enabled || cfg.hud[HUD_Keystrokes].enabled);
    if (!menuOpen && !elements) return;

    const ImVec2 display = renderer::DisplaySize();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoScrollbar;
    if (!menuOpen) flags |= ImGuiWindowFlags_NoInputs;
    ImGui::Begin("##hudlayer", nullptr, flags);

    if (elements) {
        ImVec2 pos, size;
        if (cfg.hud[HUD_Watermark].enabled) {
            BeginElement("##hudwm", HUD_Watermark, ImVec2(190, 44), menuOpen, pos, size);
            Watermark(pos, size);
        }
        if (cfg.hud[HUD_Fps].enabled) {
            BeginElement("##hudfps", HUD_Fps, ImVec2(158, 58), menuOpen, pos, size);
            Fps(pos, size);
        }
        if (cfg.hud[HUD_Cps].enabled) {
            BeginElement("##hudcps", HUD_Cps, ImVec2(120, 44), menuOpen, pos, size);
            Cps(pos, size);
        }
        if (cfg.hud[HUD_Coords].enabled) {
            BeginElement("##hudxyz", HUD_Coords, ImVec2(158, 64), menuOpen, pos, size);
            Coords(pos, size);
        }
        if (cfg.hud[HUD_Armor].enabled) {
            BeginElement("##hudarmor", HUD_Armor, ImVec2(206, 92), menuOpen, pos, size);
            Armor(pos, size);
        }
        if (cfg.hud[HUD_Keystrokes].enabled) {
            BeginElement("##hudkeys", HUD_Keystrokes, ImVec2(112, 104), menuOpen, pos, size);
            Keystrokes(pos, size);
        }
    }

    if (menuOpen && menuAnim > 0.f) {
        const char* hint = "DRAG HUD ELEMENTS TO MOVE";
        const float tw = renderer::FontSmall()->CalcTextSizeA(11.f, FLT_MAX, 0.f, hint).x;
        ImGui::GetWindowDrawList()->AddText(renderer::FontSmall(), 11.f, ImVec2((display.x - tw) * 0.5f, 12.f),
                                            Alpha(theme::Acc(1.f), 0.75f * menuAnim), hint);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

}  // namespace uv::hud
