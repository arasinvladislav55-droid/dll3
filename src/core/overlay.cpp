#include "core/overlay.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>

#include "imgui.h"
#include "imgui_internal.h"

#include "core/bridge.h"
#include "core/config.h"
#include "core/hook.h"
#include "gfx/gui.h"
#include "gfx/hud.h"
#include "gfx/renderer.h"
#include "gfx/theme.h"
#include "gfx/visuals.h"
#include "util/anim.h"
#include "util/log.h"

namespace uv::overlay {
namespace {

constexpr const char* kVersion = "1.0.0";
constexpr int kFpsHistory = 90;

struct ToastItem {
    char text[160] = {};
    float age = 0.f;
    float life = 4.2f;
};

bool g_menuOpen = false;
float g_menuAnim = 0.f;
bool g_inGame = false;
bool g_showWorld = false;

float g_dt = 1.f / 60.f;
float g_fps = 0.f;
float g_fpsAccum = 0.f;
int g_fpsFrames = 0;
float g_fpsHistory[kFpsHistory] = {};
int g_fpsCount = 0;
int g_fpsWrite = 0;
float g_historyTimer = 0.f;

std::deque<ToastItem> g_toasts;
std::deque<ULONGLONG> g_clicksL;
std::deque<ULONGLONG> g_clicksR;
int* g_bindTarget = nullptr;

float DeltaTime() {
    static LARGE_INTEGER freq{};
    static LARGE_INTEGER last{};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
        return 1.f / 60.f;
    }
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    const double seconds = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
    last = now;
    return Clamp((float)seconds, 0.0005f, 0.1f);
}

void DrawToasts() {
    if (g_toasts.empty()) return;
    const ImVec2 display = renderer::DisplaySize();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float width = 330.f;
    float y = display.y - 24.f;

    for (int i = (int)g_toasts.size() - 1; i >= 0; --i) {
        ToastItem& toast = g_toasts[(size_t)i];
        const float in = Clamp01(toast.age / 0.22f);
        const float out = Clamp01((toast.life - toast.age) / 0.45f);
        const float alpha = std::min(in, out);
        if (alpha <= 0.f) continue;

        const ImVec2 textSize = renderer::Font()->CalcTextSizeA(13.f, FLT_MAX, width - 34.f, toast.text);
        const float h = 20.f + textSize.y;
        const ImVec2 b(display.x - 22.f + (1.f - in) * 26.f, y - h);
        y -= h + 8.f;

        dl->AddRectFilled(ImVec2(b.x + 4.f, b.y + 6.f), ImVec2(b.x + width + 4.f, b.y + h + 6.f),
                          IM_COL32(0, 0, 0, (int)(70 * alpha)), 10.f);
        dl->AddRectFilled(b, ImVec2(b.x + width, b.y + h), IM_COL32(17, 17, 25, (int)(242 * alpha)), 10.f);
        dl->AddRectFilled(b, ImVec2(b.x + 3.f, b.y + h), Alpha(theme::Acc(alpha), 1.f), 10.f,
                          ImDrawFlags_RoundCornersLeft);
        dl->AddRect(b, ImVec2(b.x + width, b.y + h), IM_COL32(255, 255, 255, (int)(22 * alpha)), 10.f);

        dl->AddText(renderer::FontSmall(), 11.f, ImVec2(b.x + 16.f, b.y + 7.f),
                    Alpha(theme::Acc(alpha), 0.9f), "UNIVERSALVISUALS");
        dl->AddText(renderer::Font(), 13.f, ImVec2(b.x + 16.f, b.y + 20.f),
                    IM_COL32(232, 232, 242, (int)(240 * alpha)), toast.text);
    }
}

}  // namespace

bool Init() {
    const bool loaded = LoadConfig();
    if (!loaded) {
        // On a fresh install scale the UI to the display DPI once. The user can
        // change it later in Config -> Interface.
        g_cfg.uiScale = Clamp(renderer::DpiScale(), 1.f, 1.6f);
    }
    Log("overlay: UniversalVisuals %s ready (config=%s)", kVersion, loaded ? "loaded" : "defaults");
    return true;
}

void Shutdown() {
    if (g_cfg.autoSave) SaveConfig();
    Log("overlay: shutdown");
}

bool MenuOpen() { return g_menuOpen; }
float MenuAnim() { return g_menuAnim; }

void SetMenuOpen(bool open) {
    if (open == g_menuOpen) return;
    g_menuOpen = open;
    if (!open) {
        g_bindTarget = nullptr;
        if (g_cfg.autoSave) SaveConfig();
    }
}

void ToggleMenu() {
    SetMenuOpen(!g_menuOpen);
}

void OnEscape() {
    if (g_bindTarget) {
        g_bindTarget = nullptr;
        Toast("Keybind cancelled");
        return;
    }
    if (g_menuOpen) SetMenuOpen(false);
}

float Dt() { return g_dt; }
float Fps() { return g_fps; }
bool InGame() { return g_inGame; }
bool ShowWorld() { return g_showWorld; }
const char* Version() { return kVersion; }

float Cps(bool left) {
    std::deque<ULONGLONG>& clicks = left ? g_clicksL : g_clicksR;
    const ULONGLONG now = GetTickCount64();
    while (!clicks.empty() && now - clicks.front() > 1000) clicks.pop_front();
    return (float)clicks.size();
}

const float* FpsHistory(int& count) {
    count = g_fpsCount;
    return g_fpsHistory;
}

void Toast(const char* text) {
    if (!text) return;
    if (!g_toasts.empty() && std::strcmp(g_toasts.back().text, text) == 0) {
        g_toasts.back().age = 0.f;
        return;
    }
    ToastItem toast;
    std::snprintf(toast.text, sizeof toast.text, "%s", text);
    g_toasts.push_back(toast);
    if (g_toasts.size() > 4) g_toasts.pop_front();
}

void BeginKeybind(int* target) {
    g_bindTarget = target;
    if (target) Toast("Press a key — Esc to cancel");
}

int* BindingTarget() { return g_bindTarget; }
void CancelKeybind() { g_bindTarget = nullptr; }

void OnClick(bool left) {
    const ULONGLONG now = GetTickCount64();
    std::deque<ULONGLONG>& clicks = left ? g_clicksL : g_clicksR;
    clicks.push_back(now);
    while (!clicks.empty() && now - clicks.front() > 4000) clicks.pop_front();
}

void Render() {
    if (!renderer::Ready()) return;
    const ImVec2 client = renderer::ClientSizePx();
    if (client.x < 8.f || client.y < 8.f) return;

    g_dt = DeltaTime();
    renderer::PreFrame(g_dt);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = renderer::DisplaySize();
    const float scale = renderer::Scale();

    if (g_menuOpen && GetForegroundWindow() != renderer::Window()) SetMenuOpen(false);

    // FPS sampling (once per second for the HUD, plus a rolling graph).
    g_fpsAccum += g_dt;
    ++g_fpsFrames;
    g_historyTimer += g_dt;
    if (g_fpsAccum >= 0.5f) {
        g_fps = (float)g_fpsFrames / g_fpsAccum;
        g_fpsAccum = 0.f;
        g_fpsFrames = 0;
    }
    if (g_historyTimer >= 0.1f) {
        g_historyTimer = 0.f;
        g_fpsHistory[g_fpsWrite] = g_fps;
        g_fpsWrite = (g_fpsWrite + 1) % kFpsHistory;
        if (g_fpsCount < kFpsHistory) ++g_fpsCount;
    }

    g_menuAnim = SmoothTowards(g_menuAnim, g_menuOpen ? 1.f : 0.f, g_cfg.menuAnimSpeed, g_dt);
    if (g_menuAnim < 0.002f) g_menuAnim = 0.f;
    if (g_menuAnim > 0.998f) g_menuAnim = 1.f;

    g_inGame = bridge::Connected() || hook::CursorCaptured();
    g_showWorld = g_cfg.overlayEnabled && g_inGame;

    // Mouse position comes from the WndProc virtual cursor: the game hides and
    // clips the OS cursor while playing, raw input deltas keep us accurate.
    ImVec2 cursor;
    if (g_menuOpen && hook::VirtualCursor(cursor)) {
        io.AddMousePosEvent(cursor.x / scale, cursor.y / scale);
    } else {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
    io.MouseDrawCursor = g_menuOpen && hook::CursorCaptured();

    ImGui::NewFrame();

    theme::Update(g_dt);
    bridge::Update(g_dt);

    visuals::DrawWorld(g_showWorld);
    hud::Draw(g_showWorld, g_menuOpen, g_menuAnim);
    visuals::DrawCosmetics(g_showWorld);
    gui::Draw(g_menuOpen, g_menuAnim);

    for (ToastItem& toast : g_toasts) toast.age += g_dt;
    while (!g_toasts.empty() && g_toasts.front().age >= g_toasts.front().life) g_toasts.pop_front();
    DrawToasts();

    {
        static ULONGLONG lastLog = 0;
        if (g_menuOpen && GetTickCount64() - lastLog > 1200) {
            lastLog = GetTickCount64();
            ImGuiContext& ctx = *ImGui::GetCurrentContext();
            char order[256] = {};
            for (ImGuiWindow* w : ctx.Windows)
                std::snprintf(order + std::strlen(order), sizeof order - std::strlen(order), "%s|", w->Name);
            Log("z-order: %s", order);
        }
    }

    renderer::EndFrame();

    (void)display;
}

}  // namespace uv::overlay
