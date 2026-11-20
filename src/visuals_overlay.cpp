// Косметический OpenGL-оверлей для Minecraft Java Edition (Windows x64).
// Хукает gdi32!SwapBuffers (его вызывает GLFW/LWJGL) и рисует ImGui поверх кадра игры.
// Загрузка: System.load / -agentpath (JNI_OnLoad / Agent_OnLoad) либо обычный LoadLibrary (DllMain).
// Меню: INSERT (меняется в настройках).

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <Windows.h>
#include <GL/gl.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include "MinHook.h"

#pragma comment(lib, "opengl32.lib")

// ============================================================ конфиг
struct Config {
    int version = 3;
    // прицел
    bool crossOn = true, crossDot = true, crossOutline = true, crossSpin = false; int crossStyle = 0;
    float crossSize = 8, crossGap = 4, crossThick = 2, crossSpinSpeed = 90, crossDyn = 6;
    float crossCol[4] = {1, 1, 1, 1};
    // эффект клика
    bool burstOn = true; int burstCount = 10; float burstSpeed = 120, burstLife = 0.5f, burstGrav = 200, burstSize = 3;
    float burstCol[4] = {1, 0.6f, 0.1f, 1};
    // экран
    bool vigOn = false; float vigSize = 0.18f, vigStr = 0.5f; float vigCol[4] = {0, 0, 0, 1};
    bool glowOn = false; float glowSize = 0.2f, glowStr = 0.5f; float glowCol[4] = {0.45f, 0.35f, 1, 1};
    bool tintOn = false; float tintCol[4] = {0.2f, 0.4f, 1, 0.12f};
    bool trailOn = false; float trailLife = 0.4f, trailWidth = 4; float trailCol[4] = {0.5f, 0.4f, 1, 1};
    // HUD
    bool fpsOn = true, wmOn = true, clockOn = false, graphOn = false;
    bool ksOn = false, cpsOn = true; float ksX = 0.02f, ksY = 0.45f;
    // тема
    int menuKey = VK_INSERT; float accent[4] = {0.45f, 0.35f, 1.0f, 1.0f};
    bool rainbow = false, glow = true; float rounding = 10, menuAlpha = 0.97f, animSpeed = 6;
} cfg;

static std::wstring g_dir = L".";
static char g_status[64] = "";
static std::wstring CfgPath() { return g_dir + L"\\visuals.cfg"; }
static void SaveCfg() { if (FILE* f = _wfopen(CfgPath().c_str(), L"wb")) { fwrite(&cfg, sizeof cfg, 1, f); fclose(f); strcpy(g_status, "Сохранено"); } else strcpy(g_status, "Не удалось сохранить"); }
static void LoadCfg(bool quiet = false) {
    if (FILE* f = _wfopen(CfgPath().c_str(), L"rb")) {
        Config t; if (fread(&t, sizeof t, 1, f) == 1 && t.version == 3) { cfg = t; if (!quiet) strcpy(g_status, "Загружено"); }
        else if (!quiet) strcpy(g_status, "Файл несовместим");
        fclose(f);
    } else if (!quiet) strcpy(g_status, "Файл не найден");
}

// ============================================================ состояние
struct Particle { float x, y, vx, vy, age; };
struct TrailPt { ImVec2 p; float age; };
static std::vector<Particle> g_parts;
static std::vector<TrailPt> g_trail;
static std::vector<double> g_clicksL, g_clicksR;
static float g_pulse = 0;
static bool g_menuOpen = false;
static float g_menuAnim = 0; static ImVec2 g_menuPos(100, 60); static float g_menuAlphaNow = 1;
static int* g_bindTarget = nullptr; static HWND g_hwnd = nullptr;
static ImVec4 g_acc;
static float g_vx = 0, g_vy = 0, g_scale = 1;   // виртуальный курсор (пиксели окна) и масштаб интерфейса
static bool g_captured = false;                 // игра «захватила» мышь (мы в мире, а не в экране меню)

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
static float RowH(int t, int s, int c, int co = 0, int kb = 0, int b = 0) { return (float)(40 + t * 31 + s * 43 + c * 31 + co * 56 + kb * 35 + b * 39 + 10); }
static void BeginCard(const char* title, float w, float h) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 12});
    ImGui::BeginChild(title, {w, h}, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled({p.x, p.y + 2}, {p.x + 3, p.y + 16}, AccU(), 2);
    ImGui::GetWindowDrawList()->AddText({p.x + 10, p.y}, IM_COL32(245, 245, 255, 255), title);
    ImGui::Dummy({0, 22});
}
static void EndCard() { ImGui::EndChild(); ImGui::PopStyleVar(); }

// ============================================================ меню
static void DrawMenu() {
    static int tab = 0; static float tabAnim = 1, indY = 80;
    const char* tabs[] = {"Прицел", "Эффекты", "Экран", "HUD", "Тема", "Конфиг"};
    const int NT = 6; const ImVec2 SZ(780, 520);
    float e = 1 - powf(1 - g_menuAnim, 3.f);
    g_menuAlphaNow = e * cfg.menuAlpha;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_menuAlphaNow);
    ImGui::SetNextWindowPos({g_menuPos.x, g_menuPos.y + (1 - e) * 30}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(SZ, ImGuiCond_Always);
    ImGui::Begin("##menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImVec2 wp = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float R = cfg.rounding;

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled({0, 0}, ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, (int)(110 * e)));
    for (int i = 1; i <= 8; i++)
        bg->AddRectFilled({wp.x - i * 3.f, wp.y - i * 3.f + 6}, {wp.x + SZ.x + i * 3.f, wp.y + SZ.y + i * 3.f + 6}, IM_COL32(0, 0, 0, (int)(16 * e)), R + i * 3);
    if (cfg.glow) for (int i = 1; i <= 4; i++) bg->AddRect({wp.x - i, wp.y - i}, {wp.x + SZ.x + i, wp.y + SZ.y + i}, AccU(0.10f * e / i), R + i, 0, 1.5f);

    dl->AddRectFilled(wp, {wp.x + 170, wp.y + SZ.y}, IM_COL32(13, 13, 18, 255), R, ImDrawFlags_RoundCornersLeft);
    dl->AddText(ImGui::GetFont(), 26, {wp.x + 24, wp.y + 16}, AccU(), "VISUALS");
    dl->AddText({wp.x + 26, wp.y + 46}, IM_COL32(120, 120, 140, 255), "overlay v3 (OpenGL)");

    float targetY = 80.f + tab * 42;
    indY = Approach(indY, targetY, 16);
    dl->AddRectFilled({wp.x + 12, wp.y + indY}, {wp.x + 158, wp.y + indY + 36}, AccU(0.18f), 8);
    dl->AddRectFilled({wp.x + 12, wp.y + indY + 9}, {wp.x + 15, wp.y + indY + 27}, AccU(), 2);
    for (int i = 0; i < NT; i++) {
        ImGui::PushID(i); ImGui::SetCursorPos({12, 80.f + i * 42});
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##tab", {146, 36}) && tab != i) { tab = i; tabAnim = 0; }
        float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.f);
        hv = Approach(hv, tab == i ? 1.f : (ImGui::IsItemHovered() ? 0.55f : 0.f), 14);
        dl->AddText({p.x + 22 + 4 * hv, p.y + 9}, IM_COL32(235, 235, 245, 120 + (int)(135 * hv)), tabs[i]);
        ImGui::PopID();
    }

    dl->AddRectFilledMultiColor({wp.x + 186, wp.y + 58}, {wp.x + SZ.x - 16, wp.y + 60}, AccU(0.8f), AccU(0.0f), AccU(0.0f), AccU(0.8f));
    dl->AddText(ImGui::GetFont(), 20, {wp.x + 190, wp.y + 20}, IM_COL32(245, 245, 255, 255), tabs[tab]);
    char foot[64]; snprintf(foot, sizeof foot, "%.0f FPS", ImGui::GetIO().Framerate);
    dl->AddText({wp.x + 190, wp.y + SZ.y - 28}, IM_COL32(110, 110, 130, 255), "Esc — закрыть меню");
    dl->AddText({wp.x + SZ.x - 20 - ImGui::CalcTextSize(foot).x, wp.y + SZ.y - 28}, IM_COL32(110, 110, 130, 255), foot);

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
        SliderF("Скорость вращения", &cfg.crossSpinSpeed, 10, 360, "%.0f"); SliderF("Расширение при клике", &cfg.crossDyn, 0, 20);
        ColorRow("Цвет", cfg.crossCol);
        EndCard(); DONE break;
    case 1: LEFT
        BeginCard("Эффект клика (ЛКМ в мире)", cw, RowH(1, 5, 1));
        Toggle("Включить", &cfg.burstOn);
        SliderI("Количество", &cfg.burstCount, 2, 40); SliderF("Скорость", &cfg.burstSpeed, 30, 300, "%.0f"); SliderF("Время жизни", &cfg.burstLife, 0.2f, 2);
        SliderF("Гравитация", &cfg.burstGrav, 0, 500, "%.0f"); SliderF("Размер", &cfg.burstSize, 1, 8);
        ColorRow("Цвет", cfg.burstCol);
        EndCard();
        RIGHT
        BeginCard("След курсора меню", cw, RowH(1, 2, 1));
        Toggle("Включить", &cfg.trailOn); SliderF("Длина (сек)", &cfg.trailLife, 0.1f, 1.5f, "%.2f"); SliderF("Ширина", &cfg.trailWidth, 1, 12); ColorRow("Цвет", cfg.trailCol);
        EndCard(); DONE break;
    case 2: LEFT
        BeginCard("Виньетка", cw, RowH(1, 2, 1));
        Toggle("Включить", &cfg.vigOn); SliderF("Размер", &cfg.vigSize, 0.05f, 0.5f, "%.2f"); SliderF("Сила", &cfg.vigStr, 0.1f, 1.f, "%.2f"); ColorRow("Цвет", cfg.vigCol);
        EndCard();
        RIGHT
        BeginCard("Свечение краёв", cw, RowH(1, 2, 1));
        Toggle("Включить", &cfg.glowOn); SliderF("Размер", &cfg.glowSize, 0.05f, 0.5f, "%.2f"); SliderF("Сила", &cfg.glowStr, 0.1f, 1.f, "%.2f"); ColorRow("Цвет", cfg.glowCol);
        EndCard();
        BeginCard("Цветовой фильтр", cw, RowH(1, 0, 1));
        Toggle("Включить", &cfg.tintOn); ColorRow("Цвет и прозрачность", cfg.tintCol);
        EndCard(); DONE break;
    case 3: LEFT
        BeginCard("Информация", cw, RowH(4, 0, 0));
        Toggle("FPS", &cfg.fpsOn); Toggle("Ватермарк", &cfg.wmOn); Toggle("Часы", &cfg.clockOn); Toggle("График кадров", &cfg.graphOn);
        EndCard();
        RIGHT
        BeginCard("Клавиши и CPS", cw, RowH(2, 2, 0));
        Toggle("Показ клавиш WASD/мышь", &cfg.ksOn); Toggle("CPS", &cfg.cpsOn);
        SliderF("Позиция X", &cfg.ksX, 0, 0.9f, "%.2f"); SliderF("Позиция Y", &cfg.ksY, 0, 0.9f, "%.2f");
        EndCard(); DONE break;
    case 4: LEFT
        BeginCard("Цвета меню", cw, RowH(2, 0, 1, 0, 0, 3));
        ColorRow("Акцентный цвет", cfg.accent); Toggle("Радужный акцент", &cfg.rainbow); Toggle("Свечение окна", &cfg.glow);
        {
            struct P { const char* n; float c[3]; } pr[] = {{"Фиолетовый", {0.45f, 0.35f, 1}}, {"Циан", {0.1f, 0.8f, 0.95f}}, {"Роза", {1, 0.35f, 0.6f}}, {"Зелёный", {0.3f, 0.9f, 0.5f}}, {"Оранжевый", {1, 0.6f, 0.2f}}};
            for (int i = 0; i < 5; i++) {
                if (Button(pr[i].n, (cw - 32) / 2.f)) { cfg.accent[0] = pr[i].c[0]; cfg.accent[1] = pr[i].c[1]; cfg.accent[2] = pr[i].c[2]; }
                if (i % 2 == 0 && i < 4) ImGui::SameLine();
            }
        }
        EndCard();
        RIGHT
        BeginCard("Форма и анимация", cw, RowH(0, 3, 0));
        if (SliderF("Скругление", &cfg.rounding, 0, 18, "%.0f")) ApplyStyle();
        SliderF("Прозрачность меню", &cfg.menuAlpha, 0.5f, 1.f, "%.2f"); SliderF("Скорость анимации", &cfg.animSpeed, 2, 14, "%.0f");
        EndCard(); DONE break;
    case 5: LEFT
        BeginCard("Конфигурация", cw, RowH(0, 0, 0, 0, 1, 3));
        KeyBind("Клавиша меню", &cfg.menuKey);
        if (Button("Сохранить")) SaveCfg();
        if (Button("Загрузить")) { LoadCfg(); ApplyStyle(); }
        if (Button("Сбросить")) { cfg = Config(); ApplyStyle(); strcpy(g_status, "Сброшено"); }
        EndCard();
        RIGHT
        BeginCard("Статус", cw, 90);
        ImGui::TextDisabled("Файл: visuals.cfg"); ImGui::Text("%s", g_status);
        EndCard(); DONE break;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
}

// ============================================================ эффекты
static void DrawEdgeGlow(ImDrawList* dl, ImVec2 ds, float size, ImU32 c) {
    ImU32 c0 = c & 0x00FFFFFF; float t = ds.y * size;
    dl->AddRectFilledMultiColor({0, 0}, {ds.x, t}, c, c, c0, c0);
    dl->AddRectFilledMultiColor({0, ds.y - t}, {ds.x, ds.y}, c0, c0, c, c);
    dl->AddRectFilledMultiColor({0, 0}, {t, ds.y}, c, c0, c0, c);
    dl->AddRectFilledMultiColor({ds.x - t, 0}, {ds.x, ds.y}, c0, c, c, c0);
}

static void DrawCrosshair(ImDrawList* dl, ImVec2 c, float dt) {
    static float spin = 0; if (cfg.crossSpin) spin += dt * cfg.crossSpinSpeed * 0.01745f;
    g_pulse = std::max(0.f, g_pulse - dt * 4);
    float gap = cfg.crossGap + g_pulse * cfg.crossDyn, s = cfg.crossSize, th = cfg.crossThick;
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

static void SpawnBurst(ImVec2 c) {
    g_pulse = 1;
    if (!cfg.burstOn) return;
    for (int i = 0; i < cfg.burstCount; i++) {
        float a = (float)i / cfg.burstCount * 6.2831f + (rand() % 100) * 0.01f;
        float s = cfg.burstSpeed * (0.5f + (rand() % 100) * 0.01f);
        g_parts.push_back({c.x, c.y, cosf(a) * s, sinf(a) * s, 0});
    }
}

static void DrawEffects(float dt) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImGuiIO& io = ImGui::GetIO(); ImVec2 ds = io.DisplaySize, c(ds.x * 0.5f, ds.y * 0.5f);

    if (cfg.tintOn) dl->AddRectFilled({0, 0}, ds, CU(cfg.tintCol));
    if (cfg.vigOn) DrawEdgeGlow(dl, ds, cfg.vigSize, CU(cfg.vigCol, cfg.vigStr));
    if (cfg.glowOn) DrawEdgeGlow(dl, ds, cfg.glowSize, CU(cfg.glowCol, cfg.glowStr));

    if (cfg.trailOn && g_menuOpen) {
        static ImVec2 last(-1, -1);
        if (fabsf(io.MousePos.x - last.x) > 1 || fabsf(io.MousePos.y - last.y) > 1) { g_trail.push_back({io.MousePos, 0}); last = io.MousePos; }
    }
    for (auto& t : g_trail) t.age += dt;
    g_trail.erase(std::remove_if(g_trail.begin(), g_trail.end(), [](auto& t) { return t.age >= cfg.trailLife; }), g_trail.end());
    for (size_t i = 1; i < g_trail.size(); i++) {
        float k = 1 - g_trail[i].age / cfg.trailLife;
        dl->AddLine(g_trail[i - 1].p, g_trail[i].p, CU(cfg.trailCol, k), cfg.trailWidth * k + 0.5f);
    }

    if (cfg.crossOn) DrawCrosshair(dl, c, dt);

    for (auto& p : g_parts) {
        p.age += dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vy += cfg.burstGrav * dt;
        float k = 1 - p.age / cfg.burstLife; if (k <= 0) continue;
        dl->AddCircleFilled({p.x, p.y}, cfg.burstSize * k * 2.2f, CU(cfg.burstCol, k * 0.25f));
        dl->AddCircleFilled({p.x, p.y}, cfg.burstSize * k + 0.5f, CU(cfg.burstCol, k));
    }
    g_parts.erase(std::remove_if(g_parts.begin(), g_parts.end(), [](auto& p) { return p.age >= cfg.burstLife; }), g_parts.end());

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
}

// ============================================================ ввод и хук
typedef BOOL(WINAPI* SwapBuffers_t)(HDC);
static SwapBuffers_t oSwap = nullptr;
static WNDPROC oWndProc = nullptr;
static bool g_ready = false; static volatile LONG g_installed = 0;
static LARGE_INTEGER g_freq{}, g_last{};
typedef void(APIENTRY* glBindFramebuffer_t)(unsigned int, unsigned int);
static glBindFramebuffer_t pBindFramebuffer = nullptr;
static const unsigned GL_FRAMEBUFFER_ = 0x8D40, GL_FRAMEBUFFER_BINDING_ = 0x8CA6;

static void ClientSize(int& w, int& h) { RECT rc{}; GetClientRect(g_hwnd, &rc); w = rc.right - rc.left; h = rc.bottom - rc.top; }

// Игра в мире просит GLFW скрыть курсор и зажать его в окне. В любом экране курсор обычный.
static bool CursorCaptured() {
    CURSORINFO ci{}; ci.cbSize = sizeof(ci);
    if (GetCursorInfo(&ci)) { if (ci.hCursor == nullptr || !(ci.flags & CURSOR_SHOWING)) return true; }
    RECT clip{}, client{};
    if (GetClipCursor(&clip) && GetClientRect(g_hwnd, &client)) {
        POINT tl{client.left, client.top}, br{client.right, client.bottom};
        ClientToScreen(g_hwnd, &tl); ClientToScreen(g_hwnd, &br);
        bool matches = std::abs(clip.left - tl.x) <= 2 && std::abs(clip.top - tl.y) <= 2 && std::abs(clip.right - br.x) <= 2 && std::abs(clip.bottom - br.y) <= 2;
        bool whole = (clip.right - clip.left) >= GetSystemMetrics(SM_CXVIRTUALSCREEN) && (clip.bottom - clip.top) >= GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (matches && !whole) return true;
    }
    return false;
}
static void ClampCursor() { int w, h; ClientSize(w, h); g_vx = std::fmin((float)(w - 1), std::fmax(0.f, g_vx)); g_vy = std::fmin((float)(h - 1), std::fmax(0.f, g_vy)); }
static void OpenMenu() {
    int w, h; ClientSize(w, h); g_vx = w / 2.f; g_vy = h / 2.f; g_menuOpen = true;
    float sc = g_scale; g_menuPos = {(w / sc - 780) * 0.5f, (h / sc - 520) * 0.5f};
}
static void CloseMenu() { g_menuOpen = false; g_bindTarget = nullptr; }

static LRESULT CALLBACK hkWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    ImGuiIO& io = ImGui::GetIO();
    if (g_bindTarget && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)) { *g_bindTarget = (wp == VK_ESCAPE) ? 0 : (int)wp; g_bindTarget = nullptr; return 0; }
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lp & (1 << 30)) && (int)wp == cfg.menuKey) {
        if (g_menuOpen) { CloseMenu(); return 0; }
        if (GetForegroundWindow() == g_hwnd) { OpenMenu(); return 0; }
    }
    if (!g_menuOpen) {
        if (msg == WM_LBUTTONDOWN) { g_clicksL.push_back(GetTickCount64() / 1000.0); if (g_captured) { int w, hh; ClientSize(w, hh); SpawnBurst({w / g_scale * 0.5f, hh / g_scale * 0.5f}); } }
        else if (msg == WM_RBUTTONDOWN) g_clicksR.push_back(GetTickCount64() / 1000.0);
        return CallWindowProcW(oWndProc, h, msg, wp, lp);
    }
    switch (msg) {   // меню открыто: игра ввода не получает
    case WM_INPUT: {
        RAWINPUT ri; UINT size = sizeof(ri);
        if (g_captured && GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
            g_vx += (float)ri.data.mouse.lLastX; g_vy += (float)ri.data.mouse.lLastY; ClampCursor();
        }
        return DefWindowProcW(h, msg, wp, lp);
    }
    case WM_MOUSEMOVE:
        if (!g_captured) { g_vx = (float)(short)(lp & 0xFFFF); g_vy = (float)(short)((lp >> 16) & 0xFFFF); ClampCursor(); }
        return 0;
    case WM_LBUTTONDOWN: io.AddMouseButtonEvent(0, true); return 0;
    case WM_LBUTTONUP: io.AddMouseButtonEvent(0, false); return CallWindowProcW(oWndProc, h, msg, wp, lp);
    case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: case WM_KEYUP: case WM_SYSKEYUP:
        return CallWindowProcW(oWndProc, h, msg, wp, lp);
    case WM_MOUSEWHEEL: io.AddMouseWheelEvent(0.f, GET_WHEEL_DELTA_WPARAM(wp) / 120.f); return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: if (wp == VK_ESCAPE) CloseMenu(); return 0;
    case WM_KILLFOCUS: CloseMenu(); break;
    case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN: case WM_CHAR: case WM_SYSCHAR: return 0;
    default: break;
    }
    return CallWindowProcW(oWndProc, h, msg, wp, lp);
}

static void InitGui(HWND hwnd) {
    g_hwnd = hwnd;
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; io.MouseDrawCursor = false;
    static ImVector<ImWchar> ranges; ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic()); builder.AddText("—–•…·→✓"); builder.BuildRanges(&ranges);
    ImFontConfig fc; fc.OversampleH = 2; fc.OversampleV = 1;
    ImFont* f = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.f, &fc, ranges.Data);
    if (!f) io.Fonts->AddFontDefault();
    ImGui_ImplOpenGL3_Init("#version 150");
    ImGui_ImplOpenGL3_NewFrame();   // создаём текстуру шрифта сразу, чтобы меню не подвисало при первом открытии
    LoadCfg(true); ApplyStyle();
    oWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    QueryPerformanceFrequency(&g_freq); QueryPerformanceCounter(&g_last);
    pBindFramebuffer = (glBindFramebuffer_t)wglGetProcAddress("glBindFramebuffer");
    g_ready = true;
}

static void DrawFrame() {
    ImGuiIO& io = ImGui::GetIO(); int w, h; ClientSize(w, h); if (w <= 0 || h <= 0) return;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)((double)(now.QuadPart - g_last.QuadPart) / (double)g_freq.QuadPart); g_last = now;
    dt = std::fmin(0.1f, std::fmax(0.0001f, dt));

    bool focused = GetForegroundWindow() == g_hwnd;
    g_captured = CursorCaptured();
    if (g_menuOpen && !focused) CloseMenu();

    // в маленьком окне уменьшаем интерфейс, чтобы меню 780x520 помещалось
    g_scale = std::min(1.f, std::min((float)h / 560.f, (float)w / 820.f));
    io.DeltaTime = dt;
    io.DisplaySize = ImVec2(w / g_scale, h / g_scale);
    io.DisplayFramebufferScale = ImVec2(g_scale, g_scale);
    if (g_menuAnim > 0.001f) io.AddMousePosEvent(g_vx / g_scale, g_vy / g_scale); else io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);

    int prevFbo = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING_, &prevFbo);
    if (pBindFramebuffer) pBindFramebuffer(GL_FRAMEBUFFER_, 0);

    ImGui_ImplOpenGL3_NewFrame(); ImGui::NewFrame();
    UpdateAccent();
    g_menuAnim = ImClamp(g_menuAnim + (g_menuOpen ? 1.f : -1.f) * dt * cfg.animSpeed, 0.f, 1.f);
    DrawEffects(dt);
    if (g_menuAnim > 0.001f) {
        DrawMenu();
        if (g_captured) {   // свой курсор, т.к. системный в игре скрыт
            ImDrawList* fg = ImGui::GetForegroundDrawList(); ImVec2 p(g_vx / g_scale, g_vy / g_scale);
            fg->AddTriangleFilled(p, {p.x + 12, p.y + 14}, {p.x + 2, p.y + 18}, IM_COL32_WHITE);
            fg->AddTriangle(p, {p.x + 12, p.y + 14}, {p.x + 2, p.y + 18}, IM_COL32(0, 0, 0, 255), 1.2f);
        }
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (pBindFramebuffer) pBindFramebuffer(GL_FRAMEBUFFER_, (unsigned)prevFbo);
}

static BOOL WINAPI hkSwapBuffers(HDC dc) {
    HWND w = WindowFromDC(dc);
    if (!g_ready && w) { char cls[64] = {}; GetClassNameA(w, cls, sizeof cls); if (std::strncmp(cls, "GLFW", 4) == 0) InitGui(w); }
    if (g_ready && w == g_hwnd) DrawFrame();
    return oSwap(dc);
}

static void Install() {
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) return;   // уже установлено
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Install, &self);
    wchar_t buf[MAX_PATH] = {};
    if (GetModuleFileNameW(self, buf, MAX_PATH)) { std::wstring p(buf); size_t s = p.find_last_of(L"\\/"); g_dir = s == std::wstring::npos ? L"." : p.substr(0, s); }
    if (MH_Initialize() == MH_OK) {
        void* target = (void*)GetProcAddress(LoadLibraryA("gdi32.dll"), "SwapBuffers");
        if (target && MH_CreateHook(target, (void*)&hkSwapBuffers, (void**)&oSwap) == MH_OK) MH_EnableHook(target);
    }
}

static DWORD WINAPI InstallThread(LPVOID) { Install(); return 0; }

extern "C" __declspec(dllexport) int JNI_OnLoad(void*, void*) { Install(); return 0x00010008; }          // System.load
extern "C" __declspec(dllexport) int Agent_OnLoad(void*, char*, void*) { Install(); return 0; }          // -agentpath

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {                                                  // LoadLibrary / инжектор
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr); }
    return TRUE;
}
