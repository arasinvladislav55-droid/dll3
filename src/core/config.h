// UniversalVisuals — persisted settings.
//
// Everything here is a plain old data struct so it can be written to disk with a
// single fwrite. Add new fields at the end and bump kConfigVersion; older files are
// discarded automatically (a clean reset is better than a corrupt config).
#pragma once

#include <windows.h>

namespace uv {

constexpr int kConfigVersion = 1;
constexpr int kHudElementCount = 6;

enum HudElement : int {
    HUD_Watermark = 0,
    HUD_Fps = 1,
    HUD_Cps = 2,
    HUD_Coords = 3,
    HUD_Armor = 4,
    HUD_Keystrokes = 5,
};

struct Color3 {
    float r = 1.f, g = 1.f, b = 1.f;
};

struct HudSlot {
    bool enabled = true;
    // Normalised top-left corner (0..1 of the overlay logical size) so the layout
    // survives window resizes and fullscreen transitions.
    float x = 0.02f;
    float y = 0.02f;
};

struct Config {
    int version = kConfigVersion;

    // ---------------------------------------------------------------- general
    bool overlayEnabled = true;
    bool autoSave = true;
    int keyMenu = VK_RSHIFT;    // toggle ClickGUI
    int keyToggle = VK_F6;      // enable/disable all visuals
    int keyZoom = 'C';          // hold to zoom
    float cursorSensitivity = 1.0f;  // software cursor speed in captured mode

    // ---------------------------------------------------------------- menu
    float uiScale = 1.0f;       // logical UI zoom (0.75 .. 2.0)
    float menuOpacity = 0.97f;
    float menuAnimSpeed = 14.f;
    float accentR = 0.49f, accentG = 0.36f, accentB = 1.0f;
    bool rainbowAccent = false;

    // ---------------------------------------------------------------- HUD
    HudSlot hud[kHudElementCount] = {};
    float hudScale = 1.0f;
    bool hudBackground = true;
    bool hudShadow = true;
    bool hudBars = true;      // slim accent bars on cards
    bool hideOutsideGame = true;
    float hudSpring = 1.0f;   // keystroke animation stiffness multiplier

    // ---------------------------------------------------------------- world
    bool timeOn = false;
    int timeValue = 6000;      // Minecraft ticks (0..24000)
    bool fogOn = false;
    Color3 fogColor = {0.55f, 0.66f, 0.92f};
    float fogDensity = 0.35f;
    bool tintOn = false;
    Color3 tintColor = {0.36f, 0.30f, 0.86f};
    float tintStrength = 0.16f;
    bool zoomOn = false;
    float zoomFactor = 0.45f;  // 0.45 => 45% FOV (2.2x zoom)
    float zoomSmooth = 9.f;

    // --------------------------------------------------------------- cosmetics
    bool hatOn = true;
    float hatScale = 1.0f;
    float hatTilt = -14.f;
    float hatSpin = 26.f;      // degrees per second
    Color3 hatColorA = {0.49f, 0.36f, 1.00f};
    Color3 hatColorB = {0.18f, 0.87f, 1.00f};

    bool ringsOn = true;
    float ringLife = 0.9f;
    float ringSize = 1.0f;
    Color3 ringColor = {0.49f, 0.36f, 1.00f};

    bool trailOn = true;
    float trailRate = 26.f;    // particles per second
    float trailLife = 1.1f;
    float trailSize = 1.0f;
    Color3 trailColor = {0.58f, 0.46f, 1.00f};

    bool demoCosmetics = false;  // synthetic screen anchors, for testing only

    // ------------------------------------------------------------- animations
    bool keyAnimations = true;
    bool smoothSwing = true;
    float swingSpeed = 1.0f;

    // -------------------------------------------------------------- viewmodel
    bool viewmodelOn = false;
    float viewX = 0.f, viewY = 0.f, viewZ = 0.f;
    float viewScale = 1.f;
};

extern Config g_cfg;
// Short alias used by the UI code.
inline Config& cfg = g_cfg;

void ResetConfig();
void SanitizeConfig();
bool SaveConfig();
bool LoadConfig();
const wchar_t* ConfigPathW();
const char* ConfigPathA();

}  // namespace uv
