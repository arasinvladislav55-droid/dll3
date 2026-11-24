// UniversalVisuals — small hand-drawn ImGui widget kit.
//
// Everything is themed, animated and sized for the ClickGUI. Widgets are
// immediate-mode and return true on change.
#pragma once

#include "imgui.h"

namespace uv::widgets {

void Section(const char* title);
void Hint(const char* text);
void Gap(float height = 6.f);

bool Toggle(const char* id, bool* value, const char* label, const char* desc = nullptr);
bool SliderF(const char* id, float* value, float lo, float hi, const char* label, const char* fmt = "%.2f");
bool SliderI(const char* id, int* value, int lo, int hi, const char* label);
bool SliderPercent(const char* id, float* value, float lo, float hi, const char* label);
bool KeybindRow(const char* id, int* key, const char* label);
bool ColorRow(const char* id, float* rgb, const char* label);
bool Chip(const char* label, bool active, float width = 0.f);
bool Button(const char* label, bool accent = false, float width = 0.f);

}  // namespace uv::widgets
