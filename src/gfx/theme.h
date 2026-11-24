// UniversalVisuals — accent colour, gradients and alpha helpers.
#pragma once

#include "imgui.h"

namespace uv::theme {

// Call once per frame before drawing.
void Update(float dt);

ImVec4 Accent();
// Accent colour with alpha, as packed ImU32.
ImU32 Acc(float alpha = 1.f);
// Secondary accent (hue shifted) used for gradients.
ImU32 AccAlt(float alpha = 1.f);
// Mixes accent -> secondary accent.
ImU32 Grad(float t, float alpha = 1.f);
// Multiplies a packed colour by the current ImGui style alpha, so custom draw-list
// primitives fade together with the menu.
ImU32 Wc(ImU32 color);

int PresetCount();
const char* PresetName(int index);
ImVec4 PresetColor(int index);
void ApplyPreset(int index);

}  // namespace uv::theme
