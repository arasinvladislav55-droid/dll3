#include "gfx/widgets.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "core/overlay.h"
#include "gfx/renderer.h"
#include "gfx/theme.h"
#include "util/anim.h"

namespace uv::widgets {
namespace {

std::unordered_map<ImGuiID, float>& AnimMap() {
    static std::unordered_map<ImGuiID, float> map;
    return map;
}

float& Anim(ImGuiID id) {
    auto it = AnimMap().find(id);
    if (it == AnimMap().end()) it = AnimMap().emplace(id, 0.f).first;
    return it->second;
}

float StyleAlpha() { return ImGui::GetStyle().Alpha; }

ImU32 ColA(float r, float g, float b, float a) {
    return IM_COL32((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), (int)(a * 255.f * StyleAlpha()));
}

void Panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float fill, float border, float hover) {
    const float alpha = StyleAlpha();
    dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, (int)((fill + hover * 5.f) * 255.f * alpha)), rounding);
    dl->AddRect(a, b, IM_COL32(255, 255, 255, (int)((border + hover * 10.f) * 255.f * alpha)), rounding, 0, 1.f);
}

void DrawSwitch(ImDrawList* dl, ImVec2 pos, bool on, float t) {
    const float alpha = StyleAlpha();
    const ImVec2 a = pos;
    const ImVec2 b(pos.x + 40.f, pos.y + 22.f);
    const ImU32 offTrack = IM_COL32(255, 255, 255, (int)(22 * alpha));
    const ImU32 onTrackA = Alpha(theme::Acc(1.f), alpha);
    const ImU32 onTrackB = Alpha(theme::AccAlt(1.f), alpha);
    dl->AddRectFilled(a, b, offTrack, 11.f);
    if (t > 0.001f) {
        dl->AddRectFilled(a, b, Alpha(onTrackA, t), 11.f);
        dl->AddRectFilled(ImVec2(a.x + 19.f, a.y), b, Alpha(onTrackB, 0.55f * t), 11.f);
    }
    const float knobX = Lerp(a.x + 11.f, b.x - 11.f, t);
    const float knobY = (a.y + b.y) * 0.5f;
    dl->AddCircleFilled(ImVec2(knobX, knobY + 1.f), 8.f, IM_COL32(0, 0, 0, (int)(50 * alpha)), 24);
    dl->AddCircleFilled(ImVec2(knobX, knobY), 8.f,
                        t > 0.5f ? IM_COL32(255, 255, 255, (int)(250 * alpha))
                                 : IM_COL32(220, 220, 232, (int)(230 * alpha)), 24);
}

}  // namespace

void Gap(float height) { ImGui::Dummy(ImVec2(0.f, height)); }

void Section(const char* title) {
    Gap(4.f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ImVec2(p.x, p.y + 5.f), ImVec2(p.x + 3.f, p.y + 13.f), Alpha(theme::Acc(1.f), StyleAlpha()), 2.f);
    dl->AddText(renderer::FontSmall(), 11.f, ImVec2(p.x + 10.f, p.y + 3.f),
                Alpha(theme::Acc(1.f), StyleAlpha() * 0.95f), title);
    const float lineX = p.x + 10.f + renderer::FontSmall()->CalcTextSizeA(11.f, FLT_MAX, 0.f, title).x + 10.f;
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 16.f;
    if (right > lineX) {
        dl->AddLine(ImVec2(lineX, p.y + 9.f), ImVec2(right, p.y + 9.f),
                    IM_COL32(255, 255, 255, (int)(10 * StyleAlpha())));
    }
    ImGui::Dummy(ImVec2(0.f, 20.f));
}

void Hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.52f, 0.62f, 1.f));
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ImGui::TextWrapped("%s", text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

bool Toggle(const char* id, bool* value, const char* label, const char* desc) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = 46.f;
    const ImGuiID key = ImGui::GetID("row");

    ImGui::InvisibleButton("row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (clicked) {
        *value = !*value;
    }

    float& hover = Anim(key);
    hover = SmoothTowards(hover, hovered ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);
    float& knob = Anim(key ^ 0x5BD1E995u);
    knob = SmoothTowards(knob, *value ? 1.f : 0.f, 14.f, ImGui::GetIO().DeltaTime);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    Panel(dl, p, ImVec2(p.x + w, p.y + h), 9.f, 0.040f + (*value ? 0.012f : 0.f), 0.075f, hover);

    if (*value) {
        dl->AddRectFilled(ImVec2(p.x, p.y + 10.f), ImVec2(p.x + 2.f, p.y + h - 10.f),
                          Alpha(theme::Acc(1.f), StyleAlpha()), 1.f);
    }

    dl->AddText(renderer::Font(), 14.f, ImVec2(p.x + 13.f, p.y + (desc ? 8.f : 15.f)),
                IM_COL32(235, 235, 244, (int)(245 * StyleAlpha())), label);
    if (desc) {
        dl->AddText(renderer::FontSmall(), 11.f, ImVec2(p.x + 13.f, p.y + 26.f),
                    IM_COL32(140, 140, 158, (int)(230 * StyleAlpha())), desc);
    }
    DrawSwitch(dl, ImVec2(p.x + w - 54.f, p.y + (h - 22.f) * 0.5f), *value, knob);

    ImGui::PopID();
    return clicked;
}

bool SliderF(const char* id, float* value, float lo, float hi, const char* label, const char* fmt) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = 50.f;
    const float trackX = p.x + 13.f;
    const float trackW = w - 26.f;
    const float trackY = p.y + 36.f;

    ImGui::InvisibleButton("row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    bool changed = false;
    if (ImGui::IsItemActive()) {
        const float t = Clamp01((ImGui::GetIO().MousePos.x - trackX) / std::max(1.f, trackW));
        const float next = lo + (hi - lo) * t;
        if (std::fabs(next - *value) > 0.00001f) {
            *value = next;
            changed = true;
        }
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    const float t = Clamp01((*value - lo) / std::max(0.0001f, hi - lo));
    float& hover = Anim(ImGui::GetID("row"));
    hover = SmoothTowards(hover, hovered ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    Panel(dl, p, ImVec2(p.x + w, p.y + h), 9.f, 0.038f, 0.07f, hover);

    dl->AddText(renderer::Font(), 14.f, ImVec2(p.x + 13.f, p.y + 8.f),
                IM_COL32(235, 235, 244, (int)(245 * StyleAlpha())), label);

    char buffer[64];
    std::snprintf(buffer, sizeof buffer, fmt, *value);
    const float valueWidth = renderer::FontMedium()->CalcTextSizeA(13.f, FLT_MAX, 0.f, buffer).x;
    dl->AddText(renderer::FontMedium(), 13.f, ImVec2(p.x + w - 13.f - valueWidth, p.y + 9.f),
                Alpha(theme::Acc(1.f), StyleAlpha()), buffer);

    dl->AddRectFilled(ImVec2(trackX, trackY), ImVec2(trackX + trackW, trackY + 4.f),
                      IM_COL32(255, 255, 255, (int)(20 * StyleAlpha())), 2.f);
    if (t > 0.001f) {
        const ImVec2 fillEnd(trackX + trackW * t, trackY + 4.f);
        dl->AddRectFilledMultiColor(ImVec2(trackX, trackY), fillEnd, Alpha(theme::Acc(1.f), StyleAlpha()),
                                    Alpha(theme::Grad(1.f, 1.f), StyleAlpha()), Alpha(theme::Grad(1.f, 1.f), StyleAlpha()),
                                    Alpha(theme::Acc(1.f), StyleAlpha()));
        dl->AddCircleFilled(ImVec2(fillEnd.x, trackY + 2.f), 6.f + hover * 1.5f,
                            IM_COL32(255, 255, 255, (int)(250 * StyleAlpha())), 20);
        dl->AddCircleFilled(ImVec2(fillEnd.x, trackY + 2.f), 11.f + hover * 2.f,
                            Alpha(theme::Acc(1.f), 0.10f * StyleAlpha()), 24);
    }

    ImGui::PopID();
    return changed;
}

bool SliderI(const char* id, int* value, int lo, int hi, const char* label) {
    float f = (float)*value;
    const bool changed = SliderF(id, &f, (float)lo, (float)hi, label, "%.0f");
    if (changed) *value = (int)std::lround(f);
    return changed;
}

bool SliderPercent(const char* id, float* value, float lo, float hi, const char* label) {
    float percent = *value * 100.f;
    char text[64];
    std::snprintf(text, sizeof text, "%s", label);
    const bool changed = SliderF(id, &percent, lo * 100.f, hi * 100.f, text, "%.0f%%");
    if (changed) *value = percent / 100.f;
    return changed;
}

bool KeybindRow(const char* id, int* key, const char* label) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = 44.f;
    const ImVec2 buttonPos(p.x + w - 128.f, p.y + 9.f);

    ImGui::InvisibleButton("row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) overlay::BeginKeybind(key);

    float& hover = Anim(ImGui::GetID("row"));
    hover = SmoothTowards(hover, hovered ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);
    const bool capturing = overlay::BindingTarget() == key;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    Panel(dl, p, ImVec2(p.x + w, p.y + h), 9.f, 0.038f, 0.07f, hover);
    dl->AddText(renderer::Font(), 14.f, ImVec2(p.x + 13.f, p.y + 14.f),
                IM_COL32(235, 235, 244, (int)(245 * StyleAlpha())), label);

    const ImVec2 b(buttonPos.x + 116.f, buttonPos.y + 26.f);
    dl->AddRectFilled(buttonPos, b, capturing ? Alpha(theme::Acc(1.f), 0.20f * StyleAlpha())
                                              : IM_COL32(255, 255, 255, (int)(16 * StyleAlpha())), 7.f);
    dl->AddRect(buttonPos, b, capturing ? Alpha(theme::Acc(1.f), 0.65f * StyleAlpha())
                                        : IM_COL32(255, 255, 255, (int)(28 * StyleAlpha())), 7.f);

    char text[64];
    if (capturing) {
        std::snprintf(text, sizeof text, "press a key...");
    } else if (*key <= 0) {
        std::snprintf(text, sizeof text, "unbound");
    } else {
        const UINT scan = MapVirtualKeyA((UINT)*key, MAPVK_VK_TO_VSC) << 16;
        if (GetKeyNameTextA((LONG)scan, text, sizeof text) == 0) std::snprintf(text, sizeof text, "VK %d", *key);
    }
    const float tw = renderer::FontSmall()->CalcTextSizeA(11.5f, FLT_MAX, 0.f, text).x;
    dl->AddText(renderer::FontSmall(), 11.5f, ImVec2((buttonPos.x + b.x - tw) * 0.5f, buttonPos.y + 6.f),
                capturing ? Alpha(theme::Acc(1.f), StyleAlpha()) : IM_COL32(225, 225, 236, (int)(235 * StyleAlpha())), text);

    ImGui::PopID();
    return hovered && ImGui::IsItemClicked();
}

bool ColorRow(const char* id, float* rgb, const char* label) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = 44.f;
    const ImVec2 swatch(p.x + w - 58.f, p.y + 12.f);

    ImGui::InvisibleButton("row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    bool changed = false;
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##picker");

    float& hover = Anim(ImGui::GetID("row"));
    hover = SmoothTowards(hover, hovered ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    Panel(dl, p, ImVec2(p.x + w, p.y + h), 9.f, 0.038f, 0.07f, hover);
    dl->AddText(renderer::Font(), 14.f, ImVec2(p.x + 13.f, p.y + 14.f),
                IM_COL32(235, 235, 244, (int)(245 * StyleAlpha())), label);

    const ImVec2 swatchEnd(swatch.x + 32.f, swatch.y + 20.f);
    dl->AddRectFilledMultiColor(swatch, swatchEnd, ColA(rgb[0], rgb[1], rgb[2], 1.f),
                                ColA(rgb[0], rgb[1], rgb[2], 1.f), ColA(rgb[0], rgb[1], rgb[2], 1.f),
                                ColA(rgb[0], rgb[1], rgb[2], 1.f));
    dl->AddRect(swatch, swatchEnd, IM_COL32(255, 255, 255, (int)(45 * StyleAlpha())), 6.f);

    if (ImGui::BeginPopup("##picker")) {
        float col[4] = {rgb[0], rgb[1], rgb[2], 1.f};
        ImGui::PushItemWidth(196.f);
        if (ImGui::ColorPicker4("##color", col,
                                ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoAlpha |
                                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoLabel)) {
            rgb[0] = col[0];
            rgb[1] = col[1];
            rgb[2] = col[2];
            changed = true;
        }
        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}

bool Chip(const char* label, bool active, float width) {
    ImGui::PushID(label);
    const float h = 28.f;
    if (width <= 0.f) width = renderer::FontSmall()->CalcTextSizeA(12.f, FLT_MAX, 0.f, label).x + 26.f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("chip", ImVec2(width, h));
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const bool clicked = ImGui::IsItemClicked();
    const ImVec2 b(p.x + width, p.y + h);

    float& hover = Anim(ImGui::GetID("chip"));
    hover = SmoothTowards(hover, hovered ? 1.f : 0.f, 18.f, ImGui::GetIO().DeltaTime);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fill = active ? Alpha(theme::Acc(1.f), 0.18f * StyleAlpha())
                              : IM_COL32(255, 255, 255, (int)((7.f + hover * 5.f) * 255.f * StyleAlpha()));
    dl->AddRectFilled(p, b, fill, 8.f);
    dl->AddRect(p, b, active ? Alpha(theme::Acc(1.f), 0.55f * StyleAlpha())
                             : IM_COL32(255, 255, 255, (int)(22 * StyleAlpha())), 8.f);
    const float tw = renderer::FontSmall()->CalcTextSizeA(12.f, FLT_MAX, 0.f, label).x;
    dl->AddText(renderer::FontSmall(), 12.f, ImVec2((p.x + b.x - tw) * 0.5f, p.y + 6.5f),
                active ? Alpha(theme::Acc(1.f), StyleAlpha()) : IM_COL32(205, 205, 218, (int)(230 * StyleAlpha())), label);

    ImGui::PopID();
    return clicked;
}

bool Button(const char* label, bool accent, float width) {
    ImGui::PushID(label);
    const float h = 34.f;
    if (width <= 0.f) width = renderer::Font()->CalcTextSizeA(14.f, FLT_MAX, 0.f, label).x + 32.f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("btn", ImVec2(width, h));
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const bool clicked = ImGui::IsItemClicked();
    const ImVec2 b(p.x + width, p.y + h);

    float& hover = Anim(ImGui::GetID("btn"));
    hover = SmoothTowards(hover, hovered ? 1.f : 0.f, 16.f, ImGui::GetIO().DeltaTime);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (accent) {
        const ImU32 from = Alpha(Mix(theme::Acc(1.f), IM_COL32_WHITE, hover * 0.16f), StyleAlpha());
        const ImU32 to = Alpha(Mix(theme::AccAlt(1.f), IM_COL32_WHITE, hover * 0.12f), StyleAlpha());
        dl->AddRectFilledMultiColor(p, b, from, to, to, from);
        dl->AddText(renderer::FontMedium(), 13.5f,
                    ImVec2(p.x + (width - renderer::FontMedium()->CalcTextSizeA(13.5f, FLT_MAX, 0.f, label).x) * 0.5f,
                           p.y + 8.f),
                    IM_COL32(16, 14, 26, (int)(245 * StyleAlpha())), label);
    } else {
        dl->AddRectFilled(p, b, IM_COL32(255, 255, 255, (int)((9.f + hover * 6.f) * 255.f * StyleAlpha())), 8.f);
        dl->AddRect(p, b, IM_COL32(255, 255, 255, (int)((22.f + hover * 12.f) * 255.f * StyleAlpha())), 8.f);
        dl->AddText(renderer::Font(), 13.5f,
                    ImVec2(p.x + (width - renderer::Font()->CalcTextSizeA(13.5f, FLT_MAX, 0.f, label).x) * 0.5f,
                           p.y + 8.5f),
                    IM_COL32(225, 225, 236, (int)(240 * StyleAlpha())), label);
    }

    ImGui::PopID();
    return clicked;
}

}  // namespace uv::widgets
