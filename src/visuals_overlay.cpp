// Косметический DX11-оверлей v2 (DLL). Visual Studio, x64, C++17, флаг компилятора /utf-8.
// Зависимости: Dear ImGui (imgui, imgui_impl_win32, imgui_impl_dx11), MinHook.
// Предназначено для собственных приложений / одиночных проектов.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "MinHook.h"

#pragma comment(lib, "d3d11.lib")
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ============================================================ конфиг
struct Config {
    int version = 2;
    // прицел
    bool crossOn = true, crossDot = true, crossOutline = true, crossSpin = false; int crossStyle = 0;
    float crossSize = 8, crossGap = 4, crossThick = 2, crossSpinSpeed = 90, crossDyn = 6;
    float crossCol[4] = {1, 1, 1, 1};
    // хитмаркер
    bool hitOn = true, hitSound = false; int hitFreq = 900; float hitSize = 10, hitTime = 0.25f;
    float hitCol[4] = {1, 0.3f, 0.3f, 1};
    // числа урона
    bool dmgOn = true; float dmgLife = 1.0f, dmgRise = 60, dmgSize = 22;
    float dmgCol[4] = {1, 0.85f, 0.2f, 1}, critCol[4] = {1, 0.3f, 0.2f, 1};
    // частицы
    bool partOn = true; int partCount = 14; float partSpeed = 140, partLife = 0.6f, partGrav = 220, partSize = 3;
    float partCol[4] = {1, 0.6f, 0.1f, 1};
    // экран
    bool vigOn = true; float vigSize = 0.18f, vigStr = 0.5f; float vigCol[4] = {0, 0, 0, 1};
    bool flashOn = true; float flashStr = 0.6f; float flashCol[4] = {1, 0.1f, 0.1f, 1};
    bool lowHpOn = true; float lowHpThr = 35;
    bool tintOn = false; float tintCol[4] = {0.2f, 0.4f, 1, 0.12f};
    bool trailOn = false; float trailLife = 0.4f, trailWidth = 4; float trailCol[4] = {0.5f, 0.4f, 1, 1};
    // HUD
    bool fpsOn = true, wmOn = true, clockOn = true, graphOn = true, barsOn = true;
    bool ksOn = true, cpsOn = true; float ksX = 0.02f, ksY = 0.45f;
    bool compOn = false; float compW = 360;
    bool comboOn = true; float comboTime = 3;
    // камера
    bool zoomOn = true; int zoomKey = 'C', zoomMode = 0; float zoomFov = 0.4f, zoomSpeed = 10;
    // тема
    int menuKey = VK_INSERT; float accent[4] = {0.45f, 0.35f, 1.0f, 1.0f};
    bool rainbow = false, glow = true; float rounding = 10, menuAlpha = 0.97f, animSpeed = 6;
} cfg;

static const char* CFG_FILE = "visuals.cfg";
static char g_status[64] = "";
static void SaveCfg() { if (FILE* f = fopen(CFG_FILE, "wb")) { fwrite(&cfg, sizeof cfg, 1, f); fclose(f); strcpy(g_status, "Сохранено"); } }
static void LoadCfg() {
    if (FILE* f = fopen(CFG_FILE, "rb")) {
        Config t; if (fread(&t, sizeof t, 1, f) == 1 && t.version == 2) { cfg = t; strcpy(g_status, "Загружено"); }
        else strcpy(g_status, "Файл несовместим");
        fclose(f);
    } else strcpy(g_status, "Файл не найден");
}

// ============================================================ состояние
struct DmgNum { float x, y, val, age, drift; bool crit; };
struct Particle { float x, y, vx, vy, age; };
struct TrailPt { ImVec2 p; float age; };
static std::vector<DmgNum> g_dmg;
static std::vector<Particle> g_parts;
static std::vector<TrailPt> g_trail;
static std::vector<double> g_clicksL, g_clicksR;
static float g_hitTimer = 0, g_flash = 0, g_pulse = 0, g_killTimer = 0, g_comboTimer = 0, g_comboPop = 0;
static bool g_lastCrit = false, g_zooming = false, g_menuOpen = true;
static int g_combo = 0, g_kills = 0;
static float g_hp = 100, g_armor = 100, g_yaw = 0, g_fov = 1; static int g_ammo = 30, g_reserve = 90;
static float g_menuAnim = 0; static ImVec2 g_menuPos(200, 100); static float g_menuAlphaNow = 1;
static int* g_bindTarget = nullptr; static HWND g_hwnd = nullptr;
static ImVec4 g_acc;

// ---- экспорт: вызывается вашим приложением
extern "C" __declspec(dllexport) void Visuals_OnHit(float x, float y, float damage, bool crit) {
    g_hitTimer = cfg.hitTime; g_lastCrit = crit; g_flash = 1; g_pulse = 1;
    g_combo++; g_comboTimer = cfg.comboTime; g_comboPop = 1;
    g_dmg.push_back({x, y, damage, 0, ((rand() % 100) - 50) * 0.5f, crit});
    for (int i = 0; i < cfg.partCount; i++) {
        float a = (float)i / cfg.partCount * 6.2831f + (rand() % 100) * 0.01f;
        float s = cfg.partSpeed * (0.5f + (rand() % 100) * 0.01f);
        g_parts.push_back({x, y, cosf(a) * s, sinf(a) * s, 0});
    }
    if (cfg.hitSound) { int f = cfg.hitFreq; std::thread([f] { Beep(f, 35); }).detach(); }
}
extern "C" __declspec(dllexport) void Visuals_OnKill() { g_kills++; g_killTimer = 1.4f; }
extern "C" __declspec(dllexport) void Visuals_SetStats(float hp, float armor, int ammo, int reserve) { g_hp = hp; g_armor = armor; g_ammo = ammo; g_reserve = reserve; }
extern "C" __declspec(dllexport) void Visuals_SetYaw(float deg) { g_yaw = deg; }
extern "C" __declspec(dllexport) float Visuals_GetFovMultiplier() { return cfg.zoomOn ? g_fov : 1.0f; }
extern "C" __declspec(dllexport) void Visuals_ShowMenu(bool show) { g_menuOpen = show; }

// ============================================================ вспомогательное
static ImU32 CU(const float* c, float a = 1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * a)); }
static ImU32 AccU(float a = 1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(g_acc.x, g_acc.y, g_acc.z, a)); }
static float Approach(float cur, float target, float speed) { return cur + (target - cur) * std::min(1.f, ImGui::GetIO().DeltaTime * speed); }
static bool KeyDown(int vk) { return vk && GetForegroundWindow() == g_hwnd && (GetAsyncKeyState(vk) & 0x8000); }

static void UpdateAccent() {
    if (cfg.rainbow) { float h = fmodf((float)ImGui::GetTime() * 0.08f, 1.f); ImGui::ColorConvertHSVtoRGB(h, 0.65f, 1.f, g_acc.x, g_acc.y, g_acc.z); g_acc.w = 1; }
    else g_acc = ImVec4(cfg.accent[0], cfg.accent[1], cfg.accent[2], 1);
}

static void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = cfg.rounding; s.ChildRounding = std::max(2.f, cfg.rounding - 3); s.FrameRounding = 6;
    s.PopupRounding = 8; s.GrabRounding = 5; s.WindowBorderSize = 0; s.ChildBorderSize = 0; s.PopupBorderSize = 0;
    s.ItemSpacing = {10, 9}; s.WindowPadding = {0, 0}; s.ScrollbarSize = 6; s.ScrollbarRounding = 3;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = {0.065f, 0.065f, 0.085f, 1}; c[ImGuiCol_ChildBg] = {0.095f, 0.095f, 0.125f, 1};
    c[ImGuiCol_PopupBg] = {0.09f, 0.09f, 0.12f, 1}; c[ImGuiCol_FrameBg] = {0.13f, 0.13f, 0.17f, 1};
    c[ImGuiCol_FrameBgHovered] = {0.17f, 0.17f, 0.22f, 1}; c[ImGuiCol_FrameBgActive] = {0.17f, 0.17f, 0.22f, 1};
    c[ImGuiCol_Header] = {0.2f, 0.2f, 0.28f, 1}; c[ImGuiCol_HeaderHovered] = {0.25f, 0.25f, 0.34f, 1};
    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0}; c[ImGuiCol_ScrollbarGrab] = {0.3f, 0.3f, 0.4f, 1};
    c[ImGuiCol_Text] = {0.93f, 0.93f, 0.97f, 1}; c[ImGuiCol_TextDisabled] = {0.5f, 0.5f, 0.6f, 1};
}

// ============================================================ виджеты
static bool Toggle(const char* label, bool* v) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos(); float w = ImGui::GetContentRegionAvail().x, h = 22, tw = 40;
    bool pressed = ImGui::InvisibleButton("##t", {w, h}); if (pressed) *v = !*v;
    ImGuiStorage* st = ImGui::GetStateStorage();
    float& t = *st->GetFloatRef(ImGui::GetID("a"), *v ? 1.f : 0.f); t = Approach(t, *v ? 1.f : 0.f, 14);
    float& hv = *st->GetFloatRef(ImGui::GetID("h"), 0.f); hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 14);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText({p.x, p.y + 3}, IM_COL32(235, 235, 245, 190 + (int)(65 * hv)), label);
    ImVec2 a(p.x + w - tw, p.y + 2), b(p.x + w, p.y + h - 2); float r = (b.y - a.y) * 0.5f;
    ImVec4 off(0.19f, 0.19f, 0.25f, 1), m(off.x + (g_acc.x - off.x) * t, off.y + (g_acc.y - off.y) * t, off.z + (g_acc.z - off.z) * t, 1);
    if (t > 0.01f) dl->AddRectFilled({a.x - 2, a.y - 2}, {b.x + 2, b.y + 2}, AccU(0.18f * t), r + 2);
    dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(m), r);
    dl->AddCircleFilled({a.x + r + (tw - 2 * r) * t, a.y + r}, r - 2.5f, IM_COL32_WHITE);
    ImGui::PopID(); return pressed;
}
static bool SliderF(const char* label, float* v, float mn, float mx, const char* fmt = "%.1f") {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos(); float w = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##s", {w, 34}); bool changed = false;
    if (ImGui::IsItemActive()) { float t = ImClamp((ImGui::GetIO().MousePos.x - p.x) / w, 0.f, 1.f); float nv = mn + t * (mx - mn); if (nv != *v) { *v = nv; changed = true; } }
    float tt = (*v - mn) / (mx - mn);
    ImGuiStorage* st = ImGui::GetStateStorage();
    float& an = *st->GetFloatRef(ImGui::GetID("an"), tt); an = Approach(an, tt, 20);
    float& hv = *st->GetFloatRef(ImGui::GetID("hv"), 0.f); hv = Approach(hv, (ImGui::IsItemHovered() || ImGui::IsItemActive()) ? 1.f : 0.f, 14);
    char buf[32]; snprintf(buf, sizeof buf, fmt, *v);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p, IM_COL32(235, 235, 245, 190 + (int)(65 * hv)), label);
    dl->AddText({p.x + w - ImGui::CalcTextSize(buf).x, p.y}, IM_COL32(170, 170, 190, 255), buf);
    ImVec2 a(p.x, p.y + 24), b(p.x + w, p.y + 29);
    dl->AddRectFilled(a, b, IM_COL32(40, 40, 54, 255), 3);
    dl->AddRectFilled(a, {p.x + w * an, b.y}, AccU(), 3);
    dl->AddCircleFilled({p.x + w * an, p.y + 26.5f}, 5 + 2 * hv, IM_COL32_WHITE);
    dl->AddCircleFilled({p.x + w * an, p.y + 26.5f}, 10 * hv, AccU(0.2f * hv));
    ImGui::PopID(); return changed;
}
static void SliderI(const char* label, int* v, int mn, int mx) { float t = (float)*v; if (SliderF(label, &t, (float)mn, (float)mx, "%.0f")) *v = (int)roundf(t); }
static void ColorRow(const char* label, float* c) {
    ImGui::PushID(label); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 38);
    ImGui::ColorEdit4("##c", c, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
    ImGui::PopID();
}
static void ComboRow(const char* label, int* v, const char* items) {
    ImGui::TextUnformatted(label); ImGui::PushID(label); ImGui::SetNextItemWidth(-1); ImGui::Combo("##co", v, items); ImGui::PopID();
}
static void KeyBind(const char* label, int* key) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos(); float w = ImGui::GetContentRegionAvail().x;
    bool pr = ImGui::InvisibleButton("##k", {w, 26}); if (pr) g_bindTarget = key;
    bool listen = g_bindTarget == key; char name[32] = "—";
    if (listen) strcpy(name, "...");
    else if (*key) { UINT sc = MapVirtualKeyA(*key, MAPVK_VK_TO_VSC); if (!GetKeyNameTextA((LONG)(sc << 16), name, 32)) strcpy(name, "?"); }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText({p.x, p.y + 5}, IM_COL32(235, 235, 245, 200), label);
    ImVec2 ts = ImGui::CalcTextSize(name); float bw = std::max(60.f, ts.x + 20);
    ImVec2 a(p.x + w - bw, p.y), b(p.x + w, p.y + 26);
    dl->AddRectFilled(a, b, listen ? AccU(0.35f) : IM_COL32(34, 34, 46, 255), 6);
    dl->AddRect(a, b, AccU(listen ? 0.9f : 0.25f), 6);
    dl->AddText({a.x + (bw - ts.x) * 0.5f, p.y + 5}, IM_COL32_WHITE, name);
    ImGui::PopID();
}
static bool Button(const char* label, float w = -1) {
    ImGui::PushID(label); float aw = w < 0 ? ImGui::GetContentRegionAvail().x : w;
    ImVec2 p = ImGui::GetCursorScreenPos(); bool pr = ImGui::InvisibleButton("##b", {aw, 30});
    float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("h"), 0.f); hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 14);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + aw, p.y + 30}, AccU(0.15f + 0.3f * hv), 7);
    dl->AddRect(p, {p.x + aw, p.y + 30}, AccU(0.5f), 7);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText({p.x + (aw - ts.x) * 0.5f, p.y + 7}, IM_COL32_WHITE, label);
    ImGui::PopID(); return pr;
}
static float RowH(int t, int s, int c, int co = 0, int kb = 0, int b = 0) { return 40 + t * 31 + s * 43 + c * 31 + co * 56 + kb * 35 + b * 39 + 10; }
static void BeginCard(const char* title, float w, float h) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 12});
    ImGui::BeginChild(title, {w, h}, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouseWheel);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled({p.x, p.y + 2}, {p.x + 3, p.y + 16}, AccU(), 2);
    ImGui::GetWindowDrawList()->AddText({p.x + 10, p.y}, IM_COL32(245, 245, 255, 255), title);
    ImGui::Dummy({0, 22});
}
static void EndCard() { ImGui::EndChild(); ImGui::PopStyleVar(); }

// ============================================================ меню
static void DrawMenu() {
    static int tab = 0, prevTab = 0; static float tabAnim = 1, indY = 76;
    const char* tabs[] = {"Прицел", "Эффекты", "Экран", "HUD", "Камера", "Тема", "Конфиг"};
    const int NT = 7; const ImVec2 SZ(780, 520);
    float e = 1 - powf(1 - g_menuAnim, 3.f);
    g_menuAlphaNow = e * cfg.menuAlpha;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_menuAlphaNow);
    if (g_menuAnim < 0.999f) ImGui::SetNextWindowPos({g_menuPos.x, g_menuPos.y + (1 - e) * 30}, ImGuiCond_Always);
    else ImGui::SetNextWindowPos(g_menuPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(SZ, ImGuiCond_Always);
    ImGui::Begin("##menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouseWheel | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImVec2 wp = ImGui::GetWindowPos(); if (g_menuAnim >= 0.999f) g_menuPos = wp;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float R = cfg.rounding;

    // тень и свечение (за окном)
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled({0, 0}, ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, (int)(110 * e)));
    for (int i = 1; i <= 8; i++)
        bg->AddRectFilled({wp.x - i * 3.f, wp.y - i * 3.f + 6}, {wp.x + SZ.x + i * 3.f, wp.y + SZ.y + i * 3.f + 6}, IM_COL32(0, 0, 0, (int)(16 * e)), R + i * 3);
    if (cfg.glow) for (int i = 1; i <= 4; i++) bg->AddRect({wp.x - i, wp.y - i}, {wp.x + SZ.x + i, wp.y + SZ.y + i}, AccU(0.10f * e / i), R + i, 0, 1.5f);

    // боковая панель
    dl->AddRectFilled(wp, {wp.x + 170, wp.y + SZ.y}, IM_COL32(13, 13, 18, 255), R, ImDrawFlags_RoundCornersLeft);
    dl->AddText(ImGui::GetFont(), 26, {wp.x + 24, wp.y + 16}, AccU(), "VISUALS");
    dl->AddText({wp.x + 26, wp.y + 46}, IM_COL32(120, 120, 140, 255), "overlay v2");

    float targetY = 80.f + tab * 42;
    indY = Approach(indY, targetY, 16);
    dl->AddRectFilled({wp.x + 12, wp.y + indY}, {wp.x + 158, wp.y + indY + 36}, AccU(0.18f), 8);
    dl->AddRectFilled({wp.x + 12, wp.y + indY + 9}, {wp.x + 15, wp.y + indY + 27}, AccU(), 2);
    for (int i = 0; i < NT; i++) {
        ImGui::PushID(i); ImGui::SetCursorPos({12, 80.f + i * 42});
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##tab", {146, 36}) && tab != i) { prevTab = tab; tab = i; tabAnim = 0; }
        float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.f);
        hv = Approach(hv, tab == i ? 1.f : (ImGui::IsItemHovered() ? 0.55f : 0.f), 14);
        dl->AddText({p.x + 22 + 4 * hv, p.y + 9}, IM_COL32(235, 235, 245, 120 + (int)(135 * hv)), tabs[i]);
        ImGui::PopID();
    }

    // шапка и подвал
    dl->AddRectFilledMultiColor({wp.x + 186, wp.y + 58}, {wp.x + SZ.x - 16, wp.y + 60}, AccU(0.8f), AccU(0.0f), AccU(0.0f), AccU(0.8f));
    dl->AddText(ImGui::GetFont(), 20, {wp.x + 190, wp.y + 20}, IM_COL32(245, 245, 255, 255), tabs[tab]);
    char foot[64]; snprintf(foot, sizeof foot, "%.0f FPS", ImGui::GetIO().Framerate);
    dl->AddText({wp.x + 190, wp.y + SZ.y - 28}, IM_COL32(110, 110, 130, 255), "Колесо мыши — прокрутка вкладки");
    dl->AddText({wp.x + SZ.x - 20 - ImGui::CalcTextSize(foot).x, wp.y + SZ.y - 28}, IM_COL32(110, 110, 130, 255), foot);

    // контент
    tabAnim = Approach(tabAnim, 1, 12);
    ImGui::SetCursorPos({186, 68});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_menuAlphaNow * tabAnim);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0, 0, 0, 0});
    ImGui::BeginChild("##content", {578, 420}, false);
    ImGui::PopStyleColor();
    ImGui::Dummy({0, (1 - tabAnim) * 14});
    float cw = (ImGui::GetContentRegionAvail().x - 6 - 12) / 2.f;
#define LEFT ImGui::BeginGroup();
#define RIGHT ImGui::EndGroup(); ImGui::SameLine(0, 12); ImGui::BeginGroup();
#define DONE ImGui::EndGroup();

    switch (tab) {
    case 0: LEFT
        BeginCard("Основное", cw, RowH(4, 0, 0, 1));
        Toggle("Включить прицел", &cfg.crossOn);
        ComboRow("Стиль", &cfg.crossStyle, "Крест\0Круг\0Точка\0T-образный\0");
        Toggle("Точка в центре", &cfg.crossDot); Toggle("Обводка", &cfg.crossOutline); Toggle("Вращение", &cfg.crossSpin);
        EndCard();
        RIGHT
        BeginCard("Параметры", cw, RowH(0, 5, 1));
        SliderF("Размер", &cfg.crossSize, 2, 30); SliderF("Зазор", &cfg.crossGap, 0, 20); SliderF("Толщина", &cfg.crossThick, 1, 6);
        SliderF("Скорость вращения", &cfg.crossSpinSpeed, 10, 360, "%.0f"); SliderF("Расширение при выстреле", &cfg.crossDyn, 0, 20);
        ColorRow("Цвет", cfg.crossCol);
        EndCard(); DONE break;
    case 1: LEFT
        BeginCard("Хитмаркер", cw, RowH(2, 3, 1));
        Toggle("Включить", &cfg.hitOn); Toggle("Звук попадания", &cfg.hitSound);
        SliderF("Размер", &cfg.hitSize, 4, 30); SliderF("Длительность", &cfg.hitTime, 0.05f, 1.f, "%.2f"); SliderI("Частота звука, Гц", &cfg.hitFreq, 300, 2000);
        ColorRow("Цвет", cfg.hitCol);
        EndCard();
        BeginCard("Числа урона", cw, RowH(1, 3, 2));
        Toggle("Включить", &cfg.dmgOn);
        SliderF("Время жизни", &cfg.dmgLife, 0.3f, 3); SliderF("Подъём", &cfg.dmgRise, 10, 150, "%.0f"); SliderF("Размер текста", &cfg.dmgSize, 12, 40, "%.0f");
        ColorRow("Обычный", cfg.dmgCol); ColorRow("Критический", cfg.critCol);
        EndCard();
        RIGHT
        BeginCard("Частицы удара", cw, RowH(1, 5, 1));
        Toggle("Включить", &cfg.partOn);
        SliderI("Количество", &cfg.partCount, 2, 40); SliderF("Скорость", &cfg.partSpeed, 30, 300, "%.0f"); SliderF("Время жизни", &cfg.partLife, 0.2f, 2);
        SliderF("Гравитация", &cfg.partGrav, 0, 500, "%.0f"); SliderF("Размер", &cfg.partSize, 1, 8);
        ColorRow("Цвет", cfg.partCol);
        EndCard(); DONE break;
    case 2: LEFT
        BeginCard("Виньетка", cw, RowH(1, 2, 1));
        Toggle("Включить", &cfg.vigOn); SliderF("Размер", &cfg.vigSize, 0.05f, 0.5f, "%.2f"); SliderF("Сила", &cfg.vigStr, 0.1f, 1.f, "%.2f"); ColorRow("Цвет", cfg.vigCol);
        EndCard();
        BeginCard("Вспышка при попадании", cw, RowH(1, 1, 1));
        Toggle("Включить", &cfg.flashOn); SliderF("Сила", &cfg.flashStr, 0.1f, 1.f, "%.2f"); ColorRow("Цвет", cfg.flashCol);
        EndCard();
        RIGHT
        BeginCard("Низкое здоровье", cw, RowH(1, 1, 0));
        Toggle("Пульсирующий край", &cfg.lowHpOn); SliderF("Порог HP", &cfg.lowHpThr, 10, 80, "%.0f");
        EndCard();
        BeginCard("Цветовой фильтр", cw, RowH(1, 0, 1));
        Toggle("Включить", &cfg.tintOn); ColorRow("Цвет и прозрачность", cfg.tintCol);
        EndCard();
        BeginCard("След курсора", cw, RowH(1, 2, 1));
        Toggle("Включить", &cfg.trailOn); SliderF("Длина (сек)", &cfg.trailLife, 0.1f, 1.5f, "%.2f"); SliderF("Ширина", &cfg.trailWidth, 1, 12); ColorRow("Цвет", cfg.trailCol);
        EndCard(); DONE break;
    case 3: LEFT
        BeginCard("Информация", cw, RowH(5, 0, 0));
        Toggle("FPS", &cfg.fpsOn); Toggle("Ватермарк", &cfg.wmOn); Toggle("Часы", &cfg.clockOn); Toggle("График кадров", &cfg.graphOn); Toggle("Полосы HP / броня / патроны", &cfg.barsOn);
        EndCard();
        BeginCard("Комбо", cw, RowH(1, 1, 0));
        Toggle("Счётчик комбо и убийств", &cfg.comboOn); SliderF("Таймаут комбо, с", &cfg.comboTime, 1, 8);
        EndCard();
        RIGHT
        BeginCard("Клавиши и CPS", cw, RowH(2, 2, 0));
        Toggle("Показ клавиш WASD/мышь", &cfg.ksOn); Toggle("CPS", &cfg.cpsOn);
        SliderF("Позиция X", &cfg.ksX, 0, 0.9f, "%.2f"); SliderF("Позиция Y", &cfg.ksY, 0, 0.9f, "%.2f");
        EndCard();
        BeginCard("Компас", cw, RowH(1, 1, 0));
        Toggle("Включить", &cfg.compOn); SliderF("Ширина", &cfg.compW, 200, 700, "%.0f");
        EndCard(); DONE break;
    case 4: LEFT
        BeginCard("Зум", cw, RowH(1, 2, 0, 1, 1));
        Toggle("Включить зум", &cfg.zoomOn); KeyBind("Клавиша", &cfg.zoomKey);
        ComboRow("Режим", &cfg.zoomMode, "Удержание\0Переключатель\0");
        SliderF("Множитель FOV", &cfg.zoomFov, 0.1f, 0.9f, "%.2f"); SliderF("Плавность", &cfg.zoomSpeed, 2, 30, "%.0f");
        EndCard();
        RIGHT
        BeginCard("Интеграция", cw, 170);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cw - 36);
        ImGui::TextDisabled("Приложение читает Visuals_GetFovMultiplier() и умножает на него свой FOV. Значение плавно меняется между 1.0 и выбранным множителем.");
        ImGui::PopTextWrapPos();
        EndCard(); DONE break;
    case 5: LEFT
        BeginCard("Цвета меню", cw, RowH(2, 0, 1, 0, 0, 2));
        ColorRow("Акцентный цвет", cfg.accent); Toggle("Радужный акцент", &cfg.rainbow); Toggle("Свечение окна", &cfg.glow);
        {
            struct P { const char* n; float c[3]; } pr[] = {{"Фиолетовый", {0.45f, 0.35f, 1}}, {"Циан", {0.1f, 0.8f, 0.95f}}, {"Роза", {1, 0.35f, 0.6f}}, {"Зелёный", {0.3f, 0.9f, 0.5f}}, {"Оранжевый", {1, 0.6f, 0.2f}}};
            for (auto& p : pr) { ImVec2 s = ImGui::GetCursorScreenPos(); (void)s; if (Button(p.n, (cw - 32) / 2.f)) { cfg.accent[0] = p.c[0]; cfg.accent[1] = p.c[1]; cfg.accent[2] = p.c[2]; } if (&p != &pr[4] && ((&p - pr) % 2 == 0)) ImGui::SameLine(); }
        }
        EndCard();
        RIGHT
        BeginCard("Форма и анимация", cw, RowH(0, 3, 0));
        if (SliderF("Скругление", &cfg.rounding, 0, 18, "%.0f")) ApplyStyle();
        SliderF("Прозрачность меню", &cfg.menuAlpha, 0.5f, 1.f, "%.2f"); SliderF("Скорость анимации", &cfg.animSpeed, 2, 14, "%.0f");
        EndCard(); DONE break;
    case 6: LEFT
        BeginCard("Конфигурация", cw, RowH(0, 0, 0, 0, 1, 3));
        KeyBind("Клавиша меню", &cfg.menuKey);
        if (Button("Сохранить")) SaveCfg();
        if (Button("Загрузить")) { LoadCfg(); ApplyStyle(); }
        if (Button("Сбросить")) { cfg = Config(); ApplyStyle(); strcpy(g_status, "Сброшено"); }
        EndCard();
        RIGHT
        BeginCard("Статус", cw, 90);
        ImGui::TextDisabled("Файл: %s", CFG_FILE); ImGui::Text("%s", g_status);
        EndCard(); DONE break;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
    (void)prevTab;
}

// ============================================================ эффекты
static void DrawEdgeGlow(ImDrawList* dl, ImVec2 ds, float size, ImU32 c) {
    ImU32 c0 = c & 0x00FFFFFF; float t = ds.y * size;
    dl->AddRectFilledMultiColor({0, 0}, {ds.x, t}, c, c, c0, c0);
    dl->AddRectFilledMultiColor({0, ds.y - t}, {ds.x, ds.y}, c0, c0, c, c);
    dl->AddRectFilledMultiColor({0, 0}, {t, ds.y}, c, c0, c0, c);
    dl->AddRectFilledMultiColor({ds.x - t, 0}, {ds.x, ds.y}, c0, c, c, c0);
}
static ImU32 ScaleA(const float* c, float a) { return CU(c, ImClamp(a, 0.f, 1.f)); }

static void DrawCrosshair(ImDrawList* dl, ImVec2 c, float dt) {
    static float spin = 0; if (cfg.crossSpin) spin += dt * cfg.crossSpinSpeed * 0.01745f;
    g_pulse = std::max(0.f, g_pulse - dt * 4); float pulse = g_pulse;
    if (KeyDown(VK_LBUTTON)) pulse = std::max(pulse, 0.6f);
    float gap = cfg.crossGap + pulse * cfg.crossDyn, s = cfg.crossSize, th = cfg.crossThick;
    auto draw = [&](ImU32 col, float extra) {
        float t = th + extra;
        switch (cfg.crossStyle) {
        case 0: case 3:
            for (int k = 0; k < 4; k++) {
                if (cfg.crossStyle == 3 && k == 3) continue;
                float a = spin + k * 1.5708f, dx = cosf(a), dy = sinf(a);
                dl->AddLine({c.x + dx * gap, c.y + dy * gap}, {c.x + dx * (gap + s), c.y + dy * (gap + s)}, col, t);
            } break;
        case 1: dl->AddCircle(c, gap + s * 0.5f, col, 40, t); break;
        case 2: dl->AddCircleFilled(c, s * 0.4f + extra * 0.5f, col); break;
        }
        if (cfg.crossDot && cfg.crossStyle != 2) dl->AddCircleFilled(c, th * 0.5f + 0.5f + extra * 0.5f, col);
    };
    if (cfg.crossOutline) draw(IM_COL32(0, 0, 0, 200), 2);
    draw(CU(cfg.crossCol), 0);
}

static void DrawEffects(float dt) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO(); ImVec2 ds = io.DisplaySize, c(ds.x * 0.5f, ds.y * 0.5f);
    float now = (float)ImGui::GetTime();

    // зум (плавный)
    g_fov = Approach(g_fov, (cfg.zoomOn && g_zooming) ? cfg.zoomFov : 1.f, cfg.zoomSpeed);

    // экранные эффекты
    if (cfg.tintOn) dl->AddRectFilled({0, 0}, ds, CU(cfg.tintCol));
    if (cfg.vigOn) DrawEdgeGlow(dl, ds, cfg.vigSize, CU(cfg.vigCol, cfg.vigStr));
    if (cfg.lowHpOn && g_hp < cfg.lowHpThr) {
        float k = (1 - g_hp / cfg.lowHpThr) * (0.65f + 0.35f * sinf(now * 6));
        DrawEdgeGlow(dl, ds, 0.25f, IM_COL32(220, 20, 20, (int)(170 * ImClamp(k, 0.f, 1.f))));
    }
    if (cfg.flashOn && g_flash > 0) { DrawEdgeGlow(dl, ds, 0.2f, CU(cfg.flashCol, g_flash * cfg.flashStr)); g_flash = std::max(0.f, g_flash - dt * 3); }

    // след курсора
    if (cfg.trailOn) {
        static ImVec2 last(-1, -1);
        if (io.MousePos.x > -1000 && (fabsf(io.MousePos.x - last.x) > 1 || fabsf(io.MousePos.y - last.y) > 1)) { g_trail.push_back({io.MousePos, 0}); last = io.MousePos; }
        for (auto& t : g_trail) t.age += dt;
        g_trail.erase(std::remove_if(g_trail.begin(), g_trail.end(), [](auto& t) { return t.age >= cfg.trailLife; }), g_trail.end());
        for (size_t i = 1; i < g_trail.size(); i++) {
            float k = 1 - g_trail[i].age / cfg.trailLife;
            dl->AddLine(g_trail[i - 1].p, g_trail[i].p, CU(cfg.trailCol, k), cfg.trailWidth * k + 0.5f);
        }
    } else g_trail.clear();

    if (cfg.crossOn) DrawCrosshair(dl, c, dt);

    if (cfg.hitOn && g_hitTimer > 0) {
        float k = g_hitTimer / cfg.hitTime, s = cfg.hitSize, o = 5 + (1 - k) * 4;
        ImU32 col = g_lastCrit ? CU(cfg.critCol, k) : CU(cfg.hitCol, k);
        for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2)
            dl->AddLine({c.x + sx * o, c.y + sy * o}, {c.x + sx * (o + s), c.y + sy * (o + s)}, col, 2);
        g_hitTimer -= dt;
    }
    if (cfg.dmgOn) {
        for (auto& d : g_dmg) {
            d.age += dt; float k = 1 - d.age / cfg.dmgLife; if (k <= 0) continue;
            char b[16]; snprintf(b, sizeof b, "%.0f", d.val);
            float pop = 1 + std::max(0.f, 1 - d.age * 8) * (d.crit ? 0.9f : 0.5f);
            float sz = cfg.dmgSize * pop * (d.crit ? 1.25f : 1.f);
            ImVec2 pos(d.x + d.drift * d.age - ImGui::CalcTextSize(b).x * 0.5f, d.y - d.age * cfg.dmgRise);
            dl->AddText(ImGui::GetFont(), sz, {pos.x + 1.5f, pos.y + 1.5f}, IM_COL32(0, 0, 0, (int)(210 * k)), b);
            dl->AddText(ImGui::GetFont(), sz, pos, d.crit ? CU(cfg.critCol, k) : CU(cfg.dmgCol, k), b);
        }
        g_dmg.erase(std::remove_if(g_dmg.begin(), g_dmg.end(), [](auto& d) { return d.age >= cfg.dmgLife; }), g_dmg.end());
    }
    if (cfg.partOn) {
        for (auto& p : g_parts) {
            p.age += dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vy += cfg.partGrav * dt;
            float k = 1 - p.age / cfg.partLife; if (k <= 0) continue;
            dl->AddCircleFilled({p.x, p.y}, cfg.partSize * k * 2.2f, CU(cfg.partCol, k * 0.25f));
            dl->AddCircleFilled({p.x, p.y}, cfg.partSize * k + 0.5f, CU(cfg.partCol, k));
        }
        g_parts.erase(std::remove_if(g_parts.begin(), g_parts.end(), [](auto& p) { return p.age >= cfg.partLife; }), g_parts.end());
    }

    // комбо / убийства
    if (cfg.comboOn) {
        g_comboTimer -= dt; g_comboPop = std::max(0.f, g_comboPop - dt * 5);
        if (g_comboTimer <= 0) g_combo = 0;
        if (g_combo > 1) {
            char b[24]; snprintf(b, sizeof b, "x%d", g_combo); float sz = 30 * (1 + g_comboPop * 0.5f);
            ImVec2 ts = ImGui::CalcTextSize(b); float k = ImClamp(g_comboTimer, 0.f, 1.f);
            dl->AddText(ImGui::GetFont(), sz, {c.x - ts.x * sz / 16.f * 0.5f, ds.y * 0.62f}, AccU(k), b);
        }
        if (g_killTimer > 0) {
            g_killTimer -= dt; float k = ImClamp(g_killTimer, 0.f, 1.f); const char* t = "УБИЙСТВО";
            float sz = 28; ImVec2 ts = ImGui::CalcTextSize(t);
            dl->AddText(ImGui::GetFont(), sz, {c.x - ts.x * sz / 16.f * 0.5f, ds.y * 0.68f}, CU(cfg.critCol, k), t);
        }
    }

    // ватермарк / FPS / часы
    static float ft[120] = {}; static int fi = 0; ft[fi] = dt * 1000; fi = (fi + 1) % 120;
    if (cfg.fpsOn || cfg.wmOn || cfg.clockOn) {
        char b[96] = ""; SYSTEMTIME st; GetLocalTime(&st);
        auto add = [&](const char* s) { if (b[0]) strcat(b, "  |  "); strcat(b, s); };
        char t[32];
        if (cfg.wmOn) add("visuals");
        if (cfg.fpsOn) { snprintf(t, sizeof t, "%.0f FPS", io.Framerate); add(t); }
        if (cfg.clockOn) { snprintf(t, sizeof t, "%02d:%02d", st.wHour, st.wMinute); add(t); }
        ImVec2 ts = ImGui::CalcTextSize(b);
        dl->AddRectFilled({10, 10}, {26 + ts.x, 36}, IM_COL32(12, 12, 16, 200), 6);
        dl->AddRectFilled({10, 10}, {26 + ts.x, 13}, AccU(), 6, ImDrawFlags_RoundCornersTop);
        dl->AddText({18, 15}, IM_COL32_WHITE, b);
        if (cfg.graphOn) {
            dl->AddRectFilled({10, 42}, {170, 86}, IM_COL32(12, 12, 16, 180), 6);
            for (int i = 1; i < 120; i++) {
                float a = ft[(fi + i - 1) % 120], bb = ft[(fi + i) % 120];
                float x0 = 14 + (i - 1) * 1.3f, x1 = 14 + i * 1.3f;
                dl->AddLine({x0, 82 - ImClamp(a, 0.f, 40.f)}, {x1, 82 - ImClamp(bb, 0.f, 40.f)}, AccU(0.9f), 1.2f);
            }
        }
    }

    // клавиши и CPS
    if (cfg.ksOn) {
        static float ka[6] = {}; float x = ds.x * cfg.ksX, y = ds.y * cfg.ksY, S = 44, G = 4;
        double tn = GetTickCount64() / 1000.0;
        auto cnt = [&](std::vector<double>& v) { v.erase(std::remove_if(v.begin(), v.end(), [&](double t) { return tn - t > 1.0; }), v.end()); return (int)v.size(); };
        struct K { const char* n; int vk; ImVec2 p, sz; } keys[] = {
            {"W", 'W', {x + S + G, y}, {S, S}}, {"A", 'A', {x, y + S + G}, {S, S}}, {"S", 'S', {x + S + G, y + S + G}, {S, S}},
            {"D", 'D', {x + 2 * (S + G), y + S + G}, {S, S}}, {"ЛКМ", VK_LBUTTON, {x, y + 2 * (S + G)}, {S * 1.5f + G / 2, 40}}, {"ПКМ", VK_RBUTTON, {x + S * 1.5f + G * 1.5f, y + 2 * (S + G)}, {S * 1.5f + G / 2, 40}}};
        for (int i = 0; i < 6; i++) {
            K& k = keys[i]; ka[i] = Approach(ka[i], KeyDown(k.vk) ? 1.f : 0.f, 20);
            dl->AddRectFilled(k.p, {k.p.x + k.sz.x, k.p.y + k.sz.y}, IM_COL32(12, 12, 16, 170), 6);
            dl->AddRectFilled(k.p, {k.p.x + k.sz.x, k.p.y + k.sz.y}, AccU(0.75f * ka[i]), 6);
            char lab[24]; strcpy(lab, k.n);
            if (cfg.cpsOn && i >= 4) snprintf(lab, sizeof lab, "%s %d", k.n, cnt(i == 4 ? g_clicksL : g_clicksR));
            ImVec2 ts = ImGui::CalcTextSize(lab);
            dl->AddText({k.p.x + (k.sz.x - ts.x) * 0.5f, k.p.y + (k.sz.y - ts.y) * 0.5f}, IM_COL32_WHITE, lab);
        }
    }

    // полосы HP / броня / патроны
    if (cfg.barsOn) {
        float x = 30, y = ds.y - 70, w = 240;
        auto bar = [&](float yy, float h, float v, ImU32 a, ImU32 b2) {
            dl->AddRectFilled({x, yy}, {x + w, yy + h}, IM_COL32(12, 12, 16, 190), 4);
            dl->AddRectFilledMultiColor({x, yy}, {x + w * ImClamp(v / 100.f, 0.f, 1.f), yy + h}, a, b2, b2, a);
        };
        bar(y, 14, g_hp, IM_COL32(220, 50, 70, 255), IM_COL32(255, 120, 90, 255));
        bar(y + 20, 8, g_armor, IM_COL32(60, 120, 255, 255), IM_COL32(100, 200, 255, 255));
        char b[32]; snprintf(b, sizeof b, "%.0f", g_hp); dl->AddText({x + w + 8, y - 1}, IM_COL32_WHITE, b);
        snprintf(b, sizeof b, "%d / %d", g_ammo, g_reserve);
        ImVec2 ts = ImGui::CalcTextSize(b); dl->AddText(ImGui::GetFont(), 30, {ds.x - 40 - ts.x * 30 / 16.f, ds.y - 70}, IM_COL32_WHITE, b);
    }

    // компас
    if (cfg.compOn) {
        float w = cfg.compW, cx = c.x, y = 16;
        dl->AddRectFilled({cx - w / 2 - 8, y - 6}, {cx + w / 2 + 8, y + 30}, IM_COL32(12, 12, 16, 170), 8);
        float base = floorf(g_yaw / 15.f) * 15.f;
        for (int k = -8; k <= 8; k++) {
            float deg = base + k * 15.f, off = (deg - g_yaw) / 90.f * (w / 2); float fade = 1 - fabsf(off) / (w / 2); if (fade <= 0) continue;
            int d = ((int)deg % 360 + 360) % 360; const char* lab = d == 0 ? "N" : d == 90 ? "E" : d == 180 ? "S" : d == 270 ? "W" : nullptr;
            if (lab) { ImVec2 ts = ImGui::CalcTextSize(lab); dl->AddText({cx + off - ts.x / 2, y + 6}, IM_COL32(255, 255, 255, (int)(255 * fade)), lab); }
            else dl->AddLine({cx + off, y + 12}, {cx + off, y + 20}, IM_COL32(200, 200, 220, (int)(160 * fade)), 1.f);
        }
        dl->AddTriangleFilled({cx - 5, y - 4}, {cx + 5, y - 4}, {cx, y + 3}, AccU());
    }
}

// ============================================================ хук Present
typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
static PresentFn oPresent = nullptr; static WNDPROC oWndProc = nullptr;
static ID3D11Device* g_dev = nullptr; static ID3D11DeviceContext* g_ctx = nullptr; static bool g_init = false;

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (g_bindTarget && m == WM_KEYDOWN) { *g_bindTarget = (w == VK_ESCAPE) ? 0 : (int)w; g_bindTarget = nullptr; return 0; }
    if (m == WM_KEYDOWN && !(l & 0x40000000)) {
        if ((int)w == cfg.menuKey) g_menuOpen = !g_menuOpen;
        if ((int)w == cfg.zoomKey) g_zooming = cfg.zoomMode == 0 ? true : !g_zooming;
    }
    if (m == WM_KEYUP && (int)w == cfg.zoomKey && cfg.zoomMode == 0) g_zooming = false;
    if (m == WM_LBUTTONDOWN) g_clicksL.push_back(GetTickCount64() / 1000.0);
    if (m == WM_RBUTTONDOWN) g_clicksR.push_back(GetTickCount64() / 1000.0);
    if (g_menuOpen && ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
    return CallWindowProcW(oWndProc, h, m, w, l);
}

static HRESULT __stdcall HookedPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    if (!g_init && SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_dev))) {
        g_dev->GetImmediateContext(&g_ctx);
        DXGI_SWAP_CHAIN_DESC d; sc->GetDesc(&d); g_hwnd = d.OutputWindow;
        ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr;
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16, nullptr, io.Fonts->GetGlyphRangesCyrillic());
        ImGui_ImplWin32_Init(g_hwnd); ImGui_ImplDX11_Init(g_dev, g_ctx);
        LoadCfg(); ApplyStyle(); g_status[0] = 0;
        oWndProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
        g_init = true;
    }
    if (g_init) {
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        float dt = ImGui::GetIO().DeltaTime;
        UpdateAccent();
        g_menuAnim = ImClamp(g_menuAnim + (g_menuOpen ? 1.f : -1.f) * dt * cfg.animSpeed, 0.f, 1.f);
        ImGui::GetIO().MouseDrawCursor = g_menuAnim > 0.01f;
        DrawEffects(dt);
        if (g_menuAnim > 0.001f) DrawMenu();
        ImGui::Render();
        ID3D11Texture2D* bb = nullptr; ID3D11RenderTargetView* rtv = nullptr;
        sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
        g_dev->CreateRenderTargetView(bb, nullptr, &rtv);
        g_ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        rtv->Release(); bb->Release();
    }
    return oPresent(sc, sync, flags);
}

static DWORD WINAPI Init(LPVOID) {
    WNDCLASSEXW wc = {sizeof wc, CS_CLASSDC, DefWindowProcW, 0, 0, GetModuleHandleW(nullptr), 0, 0, 0, 0, L"vtmp", 0};
    RegisterClassExW(&wc);
    HWND tmp = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    DXGI_SWAP_CHAIN_DESC sd = {}; sd.BufferCount = 1; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = tmp; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    IDXGISwapChain* sc = nullptr; ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr; D3D_FEATURE_LEVEL fl;
    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx))) {
        void** vt = *(void***)sc; MH_Initialize();
        MH_CreateHook(vt[8], &HookedPresent, (void**)&oPresent); MH_EnableHook(MH_ALL_HOOKS);
        sc->Release(); dev->Release(); ctx->Release();
    }
    DestroyWindow(tmp); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); CreateThread(nullptr, 0, Init, nullptr, 0, nullptr); }
    return TRUE;
}
