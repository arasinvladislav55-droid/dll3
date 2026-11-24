// UniversalVisuals — process hook layer.
//
// Hooks wglSwapBuffers (and SwapBuffers as a fallback), subclasses the game
// window, owns the virtual cursor and forwards input to the ClickGUI.
#pragma once

#include <windows.h>

#include "imgui.h"

namespace uv::hook {

bool Install();
void Uninstall();
bool Installed();

HWND Window();

// The game hides + clips the cursor while the player is in a world.
bool CursorCaptured();

// Cursor position in physical client pixels. While the software cursor is in
// use the position is accumulated from raw mouse deltas, which keeps working
// even though the OS cursor is pinned by the game.
bool VirtualCursor(ImVec2& clientPixels);

}  // namespace uv::hook
