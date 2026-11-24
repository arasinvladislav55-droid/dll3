// UniversalVisuals — ImGui/OpenGL front-end: context, fonts, style and draw helpers.
#pragma once

#include <windows.h>

#include "imgui.h"

namespace uv::renderer {

bool Init(HWND window);
void Shutdown();
bool Ready();
HWND Window();

// Called once per presented frame.
void PreFrame(float dt);
void EndFrame();

float Scale();
float DpiScale();
ImVec2 DisplaySize();
ImVec2 ClientSizePx();

ImFont* FontSmall();
ImFont* Font();
ImFont* FontMedium();
ImFont* FontBig();
ImFont* FontTitle();

// ------------------------------------------------------------------ draw util
void Shadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha = 0.42f, float spread = 14.f);
void Glow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, ImU32 color, float spread = 10.f);
void GlassPanel(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha = 1.f);
void GradientText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text, ImU32 from, ImU32 to);
void TextShadow(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text, ImU32 color, float alpha = 0.55f);
void SoftCircle(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, int layers = 6);

}  // namespace uv::renderer
