#include "core/bridge.h"

#include <windows.h>

#include <cmath>
#include <cstring>

#include "core/config.h"
#include "gfx/renderer.h"
#include "util/log.h"

namespace uv::bridge {
namespace {

PlayerState g_player;
WorldState g_world;
ULONGLONG g_playerStamp = 0;
ULONGLONG g_worldStamp = 0;
float g_time = 0.f;

bool g_demo = false;

// Mutation capabilities are false until the integrator wires them. Flip these to
// true in your own game hook (or just call the Has* overrides below).
bool g_hasTime = false;
bool g_hasFog = false;
bool g_hasFov = false;
bool g_hasViewmodel = false;
float g_fov = 0.f;

bool g_loggedTime = false;
bool g_loggedFog = false;
bool g_loggedFov = false;
bool g_loggedViewModel = false;

void FillDemo() {
    const ImVec2 size = renderer::DisplaySize();
    const float t = g_time;
    const float sway = std::sin(t * 0.7f);

    g_player.valid = true;
    g_player.onGround = true;
    g_player.x = 128.5f + sway * 0.6f;
    g_player.y = 68.0f;
    g_player.z = -412.25f + std::cos(t * 0.5f) * 0.6f;
    g_player.speed = 2.4f;
    g_player.ping = 23 + (int)(4.f + 4.f * std::sin(t * 0.9f));

    g_player.head.valid = true;
    g_player.head.screen = ImVec2(size.x * 0.5f + sway * size.y * 0.012f, size.y * 0.285f);
    g_player.head.radius = size.y * 0.058f;

    g_player.feet.valid = true;
    g_player.feet.screen = ImVec2(size.x * 0.5f + sway * size.y * 0.018f, size.y * 0.635f);
    g_player.feet.radius = size.y * 0.105f;

    static const char* kDemoArmor[4] = {"Netherite Helmet", "Elytra", "Netherite Leggings", "Netherite Boots"};
    static const float kDemoDura[4] = {0.82f, 0.41f, 0.66f, 0.93f};
    for (int i = 0; i < 4; ++i) {
        g_player.armor[i].present = true;
        std::strncpy(g_player.armor[i].name, kDemoArmor[i], sizeof(g_player.armor[i].name) - 1);
        g_player.armor[i].durability = kDemoDura[i];
    }

    g_world.valid = true;
    g_world.timeOfDay = g_cfg.timeValue;
}

void LogOnce(bool& flag, const char* what) {
    if (flag) return;
    flag = true;
    Log("bridge: %s requested, but no game hook is wired (see core/bridge.cpp)", what);
}

}  // namespace

void Update(float dt) {
    g_time += dt;
    g_demo = g_cfg.demoCosmetics;
    if (g_demo) {
        FillDemo();
        g_playerStamp = GetTickCount64();
        g_worldStamp = g_playerStamp;
    } else {
        const ULONGLONG now = GetTickCount64();
        if (g_playerStamp && now - g_playerStamp > 1000) g_player.valid = false;
        if (g_worldStamp && now - g_worldStamp > 1000) g_world.valid = false;
    }
}

const PlayerState& GetPlayer() { return g_player; }
const WorldState& GetWorld() { return g_world; }

bool Connected() {
    const ULONGLONG now = GetTickCount64();
    return (g_playerStamp && now - g_playerStamp <= 1000) || g_demo;
}

bool Demo() { return g_demo; }

void SubmitPlayer(const PlayerState& state) {
    g_player = state;
    g_player.valid = true;
    g_playerStamp = GetTickCount64();
}

void SubmitWorld(const WorldState& state) {
    g_world = state;
    g_world.valid = true;
    g_worldStamp = GetTickCount64();
}

void Reset() {
    g_player = PlayerState{};
    g_world = WorldState{};
    g_playerStamp = 0;
    g_worldStamp = 0;
}

void SetWorldTime(int ticks) {
    LogOnce(g_loggedTime, "SetWorldTime");
    g_hasTime = false;  // <- wire me
    (void)ticks;
}

void SetFog(float r, float g, float b, float density) {
    LogOnce(g_loggedFog, "SetFog");
    g_hasFog = false;  // <- wire me
    (void)r; (void)g; (void)b; (void)density;
}

void SetFov(float degrees) {
    LogOnce(g_loggedFov, "SetFov");
    g_hasFov = false;  // <- wire me
    g_fov = degrees;
}

float GetFov() { return g_fov; }

void ApplyViewModel(float x, float y, float z, float scale, float swing) {
    LogOnce(g_loggedViewModel, "ApplyViewModel");
    g_hasViewmodel = false;  // <- wire me
    (void)x; (void)y; (void)z; (void)scale; (void)swing;
}

bool HasTimeControl() { return g_hasTime; }
bool HasFogControl() { return g_hasFog; }
bool HasFovControl() { return g_hasFov; }
bool HasViewModelControl() { return g_hasViewmodel; }

}  // namespace uv::bridge
