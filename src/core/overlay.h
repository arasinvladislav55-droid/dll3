// UniversalVisuals — frame orchestration.
//
// The hook calls Render() once per presented frame. Everything that is not
// swapped (config, bridge, menu state, click history, toasts) lives here so the
// rendering modules never talk to each other directly.
#pragma once

namespace uv::overlay {

bool Init();
void Shutdown();
void Render();

bool MenuOpen();
void SetMenuOpen(bool open);
void ToggleMenu();
float MenuAnim();

// True when the player is actually in a world (cursor grabbed, or game data is
// flowing through core/bridge). Used to hide the HUD in title screens/menus.
bool InGame();
bool ShowWorld();

float Dt();
float Fps();
float Cps(bool left);
const float* FpsHistory(int& count);

void Toast(const char* text);

// Keybind capture: the next key press is written to *target (0 = unbound).
void BeginKeybind(int* target);
int* BindingTarget();
void CancelKeybind();

void OnClick(bool left);
void OnEscape();

const char* Version();

}  // namespace uv::overlay
