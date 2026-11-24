// UniversalVisuals — GameBridge.
//
// The overlay is deliberately decoupled from any specific Minecraft version.
// Everything that needs game knowledge goes through this tiny interface:
//
//   * your mod / JNI code calls SubmitPlayer() + SubmitWorld() every tick,
//     filling screen anchors and gameplay values;
//   * the overlay calls SetWorldTime() / SetFog() / SetFov() / ApplyViewModel()
//     when the user changes a setting.
//
// Only the Submit* side needs real offsets. Until it is wired, HUD coordinates
// and armour are shown as placeholders, and cosmetics can run in Demo mode.
#pragma once

#include "imgui.h"

namespace uv::bridge {

struct Anchor {
    bool valid = false;
    ImVec2 screen = ImVec2(0, 0);  // same logical space as the HUD (renderer::DisplaySize)
    float radius = 0.f;            // world radius converted to pixels
};

struct ArmorSlot {
    bool present = false;
    char name[32] = {};
    float durability = 0.f;  // 0..1, used for the durability bar
};

struct PlayerState {
    bool valid = false;
    float x = 0.f, y = 0.f, z = 0.f;
    float yaw = 0.f, pitch = 0.f;
    bool onGround = true;
    float speed = 0.f;  // blocks per second, horizontal
    Anchor head;        // screen anchor above the local player's head
    Anchor feet;        // screen anchor at the local player's feet
    ArmorSlot armor[4]; // helmet, chestplate, leggings, boots
    int ping = -1;
};

struct WorldState {
    bool valid = false;
    int timeOfDay = 6000;  // 0..24000
    bool raining = false;
};

void Update(float dt);
const PlayerState& GetPlayer();
const WorldState& GetWorld();
bool Connected();  // real data arrived within the last second
bool Demo();

// ---------------------------------------------------------------- integration
// Call these from your game hook (JNI tick, mixin, etc.). Thread-safe enough for
// the usual "write once per tick, read on render" pattern.
void SubmitPlayer(const PlayerState& state);
void SubmitWorld(const WorldState& state);

// Clears submitted data (e.g. on disconnect).
void Reset();

// ------------------------------------------------------------ game mutations
// TODO(1.19-1.21): wire these to your target version. They are intentionally
// no-ops right now; each one logs a single hint the first time it is used.
void SetWorldTime(int ticks);
void SetFog(float r, float g, float b, float density);
void SetFov(float degrees);
float GetFov();
void ApplyViewModel(float x, float y, float z, float scale, float swing);

// True when a mutation actually reached the game (used by the UI to show a hint).
bool HasTimeControl();
bool HasFogControl();
bool HasFovControl();
bool HasViewModelControl();

}  // namespace uv::bridge
