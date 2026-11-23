// Visuals Overlay v5 — косметический OpenGL-оверлей для Minecraft Java Edition (Windows x64).
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
#include <psapi.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include "MinHook.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "psapi.lib")

// ============================================================ конфиг
struct Config {
    int version = 5;
    bool enabled = true;                 // мастер-выключатель оверлея

    // --- прицел
    bool crossOn = true;
    int  crossStyle = 0, crossPreset = -1;
    bool crossDot = true, crossOutline = true, crossSpin = false;
    bool crossGlow = true, crossRainbow = false, crossUseAccent = false;
    bool crossHideMenu = false;
    float crossSize = 8, crossGap = 4, crossThick = 2, crossDotSize = 2.2f;
    float crossOpacity = 1, crossSpinSpeed = 60, crossDyn = 6, crossGlowStr = 0.55f;
    float crossCol[4] = {1, 1, 1, 1};
    float crossGlowCol[4] = {0.55f, 0.45f, 1, 1};

    // --- эффект клика
    bool burstOn = true;
    int  burstCount = 12, burstShape = 2;
    float burstSpeed = 130, burstLife = 0.55f, burstGrav = 180, burstSize = 3;
    float burstCol[4] = {1, 0.6f, 0.1f, 1};
    bool ringOn = true;
    float ringSize = 46, ringWidth = 2.5f, ringSpeed = 1.6f;
    float ringCol[4] = {1, 0.6f, 0.1f, 1};
    bool flashOn = false;
    float flashStr = 0.35f;
    float flashCol[4] = {0.45f, 0.35f, 1, 1};

    // --- экран
    bool vigOn = false;
    float vigSize = 0.18f, vigStr = 0.5f;
    float vigCol[4] = {0, 0, 0, 1};
    bool glowOn = false;
    float glowSize = 0.2f, glowStr = 0.5f;
    float glowCol[4] = {0.45f, 0.35f, 1, 1};
    bool tintOn = false;
    float tintCol[4] = {0.2f, 0.4f, 1, 0.12f};
    bool bloomOn = false;
    float bloomSize = 0.11f, bloomStr = 0.35f;
    float bloomCol[4] = {0.55f, 0.45f, 1, 1};
    bool dustOn = false;
    int dustCount = 60;
    float dustSpeed = 12, dustSize = 1.6f;
    float dustCol[4] = {0.75f, 0.82f, 1, 0.5f};

    // --- след курсора в меню
    bool trailOn = false;
    float trailLife = 0.4f, trailWidth = 4;
    float trailCol[4] = {0.5f, 0.4f, 1, 1};

    // --- HUD
    bool wmOn = true, wmFps = true, wmClock = true, wmDate = false;
    bool graphOn = true, graphFill = true;
    bool ksOn = false, cpsOn = true, ksSpace = true, ksShift = true;
    bool listOn = true, listBg = true;
    bool sysOn = false;
    bool notifOn = true;
    float hudScale = 1.0f;
    float wmX = 0.012f, wmY = 0.012f;
    float graphX = 0.012f, graphY = 0.072f;
    float ksX = 0.02f, ksY = 0.56f;
    float listX = 0.988f, listY = 0.12f;
    float sysX = 0.012f, sysY = 0.31f;

    // --- тема / меню
    int menuKey = VK_INSERT, toggleKey = VK_RSHIFT, panicKey = VK_DELETE, styleKey = VK_F6;
    float accent[4] = {0.45f, 0.35f, 1.0f, 1.0f};
    bool rainbow = false, glow = true;
    float rounding = 10, menuAlpha = 0.97f, animSpeed = 6;
    float dim = 0.55f;
    bool autoSave = true;
    bool hideInMenus = true;             // показывать HUD/эффекты только в мире (не в меню игры)
} cfg;

// ============================================================ файлы конфига
static std::wstring g_dir = L".";
static char g_status[96] = "Готово";
static std::wstring SlotPath(int slot) {
    if (slot <= 0) return g_dir + L"\\visuals.cfg";
    return g_dir + L"\\visuals_" + std::to_wstring(slot) + L".cfg";
}
static bool SaveCfg(int slot = 0, bool quiet = false) {
    if (FILE* f = _wfopen(SlotPath(slot).c_str(), L"wb")) {
        fwrite(&cfg, sizeof cfg, 1, f);
        fclose(f);
        if (!quiet) snprintf(g_status, sizeof g_status, "Сохранено: %s", slot ? ("слот " + std::to_string(slot)).c_str() : "visuals.cfg");
        return true;
    }
    if (!quiet) strcpy(g_status, "Не удалось сохранить");
    return false;
}
static bool LoadCfg(int slot = 0, bool quiet = false) {
    if (FILE* f = _wfopen(SlotPath(slot).c_str(), L"rb")) {
        Config t;
        if (fread(&t, sizeof t, 1, f) == 1 && t.version == 5) {
            cfg = t;
            if (!quiet) snprintf(g_status, sizeof g_status, "Загружено: %s", slot ? ("слот " + std::to_string(slot)).c_str() : "visuals.cfg");
            fclose(f);
            return true;
        }
        fclose(f);
        if (!quiet) strcpy(g_status, "Файл несовместим");
        return false;
    }
    if (!quiet) strcpy(g_status, "Файл не найден");
    return false;
}

// ============================================================ состояние
struct Particle { float x, y, vx, vy, age, life; };
struct RingFx { float age, life; };
struct Dust { float x, y, vx, vy, ph, sz; };
struct TrailPt { ImVec2 p; float age; };
struct ToastMsg { std::string text; float t = 0, life = 3.f; };

static std::vector<Particle> g_parts;
static std::vector<RingFx> g_rings;
static std::vector<Dust> g_dust;
static std::vector<TrailPt> g_trail;
static std::vector<double> g_clicksL, g_clicksR;
static std::vector<ToastMsg> g_toasts;
static std::map<std::string, float> g_modAnim;

static float g_pulse = 0;          // импульс прицела от клика
static float g_flash = 0;          // вспышка краёв
static float g_stylePop = 0;       // анимация смены стиля
static float g_previewPulse = 0;   // пульс в предпросмотре
static float g_spin = 0;           // текущий угол вращения

static bool g_menuOpen = false;
static int g_menuTab = 0;
static float g_menuTabAnim = 1;
static float g_menuAnim = 0;
static ImVec2 g_menuPos(100, 60);
static float g_menuAlphaNow = 1;
static int* g_bindTarget = nullptr;
static HWND g_hwnd = nullptr;
static ImVec4 g_acc;
static float g_vx = 0, g_vy = 0, g_scale = 1;
static bool g_captured = false;
static bool g_inGame = false;       // курсор захвачен игрой = игрок в мире, а не в меню
static ULONGLONG g_startMs = 0;
static bool g_firstFrame = true;

static ImFont* g_font = nullptr;
static ImFont* g_fontBig = nullptr;
static ImFont* g_fontSmall = nullptr;

// ============================================================ вспомогательное
static float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static ImU32 CU(const float* c, float a = 1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * a)); }
static ImU32 AccU(float a = 1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(g_acc.x, g_acc.y, g_acc.z, a)); }
static float Approach(float cur, float target, float speed) { return cur + (target - cur) * std::min(1.f, ImGui::GetIO().DeltaTime * speed); }
static bool KeyDown(int vk) { return vk && GetForegroundWindow() == g_hwnd && (GetAsyncKeyState(vk) & 0x8000); }
static void KeyName(int vk, char* out, int n) {
    if (!vk) { snprintf(out, n, "—"); return; }
    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    LONG lp = (LONG)(sc << 16);
    switch (vk) {
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_RCONTROL: case VK_RMENU:
    case VK_NUMLOCK: case VK_SNAPSHOT: case VK_DIVIDE: case VK_LWIN: case VK_RWIN: case VK_APPS:
        lp |= (1 << 24);
        break;
    default:
        break;
    }
    if (!GetKeyNameTextA(lp, out, n)) snprintf(out, n, "?");
}
static void Toast(const char* fmt, ...) {
    char b[160];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    g_toasts.push_back({b});
    if (g_toasts.size() > 5) g_toasts.erase(g_toasts.begin());
}
static int CountClicks(std::vector<double>& v) {
    double now = GetTickCount64() / 1000.0;
    v.erase(std::remove_if(v.begin(), v.end(), [&](double t) { return now - t > 1.0; }), v.end());
    return (int)v.size();
}
static void UpdateAccent() {
    if (cfg.rainbow) {
        float h = fmodf((float)ImGui::GetTime() * 0.08f, 1.f);
        ImGui::ColorConvertHSVtoRGB(h, 0.65f, 1.f, g_acc.x, g_acc.y, g_acc.z);
        g_acc.w = 1;
    } else {
        g_acc = ImVec4(cfg.accent[0], cfg.accent[1], cfg.accent[2], 1);
    }
    ImGuiStyle& st = ImGui::GetStyle();
    st.Colors[ImGuiCol_CheckMark] = g_acc;
    st.Colors[ImGuiCol_SliderGrab] = g_acc;
    st.Colors[ImGuiCol_SliderGrabActive] = g_acc;
}
static void AddTextGradient(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text, ImU32 ca, ImU32 cb) {
    if (!font || !text) return;
    ImVec4 a = ImGui::ColorConvertU32ToFloat4(ca), b = ImGui::ColorConvertU32ToFloat4(cb);
    int n = (int)strlen(text);
    float x = pos.x;
    for (int i = 0; i < n; i++) {
        char ch[2] = {text[i], 0};
        float t = n > 1 ? (float)i / (float)(n - 1) : 0;
        ImVec4 c(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
        dl->AddText(font, size, ImVec2(x, pos.y), ImGui::ColorConvertFloat4ToU32(c), ch);
        x += font->CalcTextSizeA(size, FLT_MAX, 0.f, ch).x;
    }
}

static void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = cfg.rounding;
    s.ChildRounding = 9;
    s.FrameRounding = 6;
    s.PopupRounding = 8;
    s.GrabRounding = 4;
    s.ScrollbarRounding = 4;
    s.TabRounding = 6;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.PopupBorderSize = 1;
    s.WindowPadding = {0, 0};
    s.FramePadding = {8, 5};
    s.ItemSpacing = {10, 8};
    s.ItemInnerSpacing = {6, 4};
    s.ScrollbarSize = 7;
    s.GrabMinSize = 10;
    s.AntiAliasedLines = true;
    s.AntiAliasedFill = true;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = {0.055f, 0.055f, 0.075f, 1};
    c[ImGuiCol_ChildBg] = {0.085f, 0.085f, 0.110f, 0.94f};
    c[ImGuiCol_PopupBg] = {0.075f, 0.075f, 0.100f, 1};
    c[ImGuiCol_FrameBg] = {0.13f, 0.13f, 0.17f, 1};
    c[ImGuiCol_FrameBgHovered] = {0.17f, 0.17f, 0.22f, 1};
    c[ImGuiCol_FrameBgActive] = {0.19f, 0.19f, 0.25f, 1};
    c[ImGuiCol_Header] = {0.20f, 0.20f, 0.28f, 1};
    c[ImGuiCol_HeaderHovered] = {0.25f, 0.25f, 0.34f, 1};
    c[ImGuiCol_HeaderActive] = {0.28f, 0.28f, 0.38f, 1};
    c[ImGuiCol_Button] = {0.16f, 0.16f, 0.22f, 1};
    c[ImGuiCol_ButtonHovered] = {0.22f, 0.22f, 0.30f, 1};
    c[ImGuiCol_ButtonActive] = {0.26f, 0.26f, 0.35f, 1};
    c[ImGuiCol_SliderGrab] = {0.45f, 0.35f, 1, 1};
    c[ImGuiCol_CheckMark] = {0.45f, 0.35f, 1, 1};
    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab] = {0.28f, 0.28f, 0.38f, 0.9f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.38f, 0.38f, 0.50f, 1};
    c[ImGuiCol_Separator] = {1, 1, 1, 0.08f};
    c[ImGuiCol_Text] = {0.93f, 0.93f, 0.97f, 1};
    c[ImGuiCol_TextDisabled] = {0.50f, 0.50f, 0.60f, 1};
    c[ImGuiCol_Border] = {1, 1, 1, 0.06f};
}
// ============================================================ карточки и виджеты
struct CardState { ImVec2 startScreen; float startY, x, w; const char* title; bool active; };
static CardState g_card{};
static ImDrawList* g_cardDl = nullptr;
static float CW() { return g_card.active ? g_card.w - 28.f : ImGui::GetContentRegionAvail().x; }

// Карточка с динамической высотой: фон рисуется в отдельном канале draw-list уже после контента.
static void BeginCard(const char* title, float x, float y, float w) {
    ImGui::PushID(title);
    ImGui::SetCursorPos(ImVec2(x, y));
    ImVec2 sp = ImGui::GetCursorScreenPos();
    g_card = {sp, y, x, w, title, true};
    g_cardDl = ImGui::GetWindowDrawList();
    g_cardDl->ChannelsSplit(2);
    g_cardDl->ChannelsSetCurrent(1);
    ImGui::Dummy(ImVec2(w, 26));       // место под заголовок
    ImGui::Indent(x + 14);
}

static float EndCard() {
    ImGui::Unindent(g_card.x + 14);
    float endY = ImGui::GetCursorPosY() + 7.f;
    ImVec2 a = g_card.startScreen;
    ImVec2 b(a.x + g_card.w, a.y + (endY - g_card.startY));
    ImDrawList* dl = g_cardDl;
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(ImVec2(a.x + 1, a.y + 4), ImVec2(b.x + 1, b.y + 4), IM_COL32(0, 0, 0, 80), 11);
    dl->AddRectFilled(a, b, IM_COL32(17, 17, 24, 236), 10);
    dl->AddRect(a, b, IM_COL32(255, 255, 255, 14), 10);
    dl->AddRectFilled(ImVec2(a.x + 13, a.y + 11), ImVec2(a.x + 16, a.y + 25), AccU(0.9f), 2);
    dl->AddText(ImVec2(a.x + 24, a.y + 9), IM_COL32(240, 240, 250, 255), g_card.title);
    dl->ChannelsMerge();
    ImGui::PopID();
    float x = g_card.x;
    g_card.active = false;
    ImGui::SetCursorPos(ImVec2(x, endY));
    return endY;
}

static bool Toggle(const char* label, bool* v) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = CW(), h = 24, tw = 38;
    bool pressed = ImGui::InvisibleButton("##t", ImVec2(w, h));
    if (pressed) { *v = !*v; }
    ImGuiStorage* st = ImGui::GetStateStorage();
    float& t = *st->GetFloatRef(ImGui::GetID("a"), *v ? 1.f : 0.f);
    t = Approach(t, *v ? 1.f : 0.f, 14);
    float& hv = *st->GetFloatRef(ImGui::GetID("h"), 0.f);
    hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 14);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(p.x, p.y + 4), IM_COL32(235, 235, 245, (int)(185 + 70 * hv)), label);
    ImVec2 a(p.x + w - tw, p.y + 3), b(p.x + w, p.y + h - 3);
    float r = (b.y - a.y) * 0.5f;
    ImVec4 off(0.19f, 0.19f, 0.25f, 1);
    ImVec4 m(off.x + (g_acc.x - off.x) * t, off.y + (g_acc.y - off.y) * t, off.z + (g_acc.z - off.z) * t, 1);
    if (t > 0.01f) dl->AddRectFilled(ImVec2(a.x - 3, a.y - 3), ImVec2(b.x + 3, b.y + 3), AccU(0.14f * t), r + 3);
    dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(m), r);
    dl->AddCircleFilled(ImVec2(a.x + r + (tw - 2 * r) * t, a.y + r), r - 3.2f, IM_COL32_WHITE);
    ImGui::PopID();
    return pressed;
}

static bool SliderF(const char* label, float* v, float mn, float mx, const char* fmt = "%.1f") {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = CW();
    ImGui::InvisibleButton("##s", ImVec2(w, 36));
    bool changed = false;
    if (ImGui::IsItemActive()) {
        float t = ImClamp((ImGui::GetIO().MousePos.x - p.x) / w, 0.f, 1.f);
        float nv = mn + t * (mx - mn);
        if (nv != *v) { *v = nv; changed = true; }
    }
    float tt = ImClamp((*v - mn) / (mx - mn), 0.f, 1.f);
    ImGuiStorage* st = ImGui::GetStateStorage();
    float& an = *st->GetFloatRef(ImGui::GetID("an"), tt);
    an = Approach(an, tt, 20);
    float& hv = *st->GetFloatRef(ImGui::GetID("hv"), 0.f);
    hv = Approach(hv, (ImGui::IsItemHovered() || ImGui::IsItemActive()) ? 1.f : 0.f, 14);
    char buf[32];
    snprintf(buf, sizeof buf, fmt, *v);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p, IM_COL32(235, 235, 245, (int)(185 + 70 * hv)), label);
    dl->AddText(ImVec2(p.x + w - ImGui::CalcTextSize(buf).x, p.y), IM_COL32(170, 170, 192, 255), buf);
    ImVec2 a(p.x, p.y + 25), b(p.x + w, p.y + 30);
    dl->AddRectFilled(a, b, IM_COL32(38, 38, 52, 255), 3);
    dl->AddRectFilled(a, ImVec2(p.x + w * an, b.y), AccU(), 3);
    ImVec2 knob(p.x + w * an, p.y + 27.5f);
    dl->AddCircleFilled(knob, 9 * hv, AccU(0.18f * hv));
    dl->AddCircleFilled(knob, 5 + 2 * hv, IM_COL32_WHITE);
    ImGui::PopID();
    return changed;
}
static void SliderI(const char* label, int* v, int mn, int mx) {
    float t = (float)*v;
    if (SliderF(label, &t, (float)mn, (float)mx, "%.0f")) *v = (int)roundf(t);
}
static void ComboRow(const char* label, int* v, const char* items) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(CW());
    ImGui::Combo("##co", v, items);
    ImGui::PopID();
}
static void ColorRow(const char* label, float* c) {
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    float bx = g_card.active ? (g_card.x + 14 + CW() - 36) : (ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 36);
    ImGui::SameLine(bx);
    ImGui::ColorEdit4("##c", c, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
    ImGui::PopID();
}
static void KeyBind(const char* label, int* key) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = CW();
    bool pr = ImGui::InvisibleButton("##k", ImVec2(w, 26));
    if (pr) g_bindTarget = key;
    bool listen = (g_bindTarget == key);
    char name[32];
    if (listen) strcpy(name, "...");
    else KeyName(*key, name, sizeof name);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(p.x, p.y + 5), IM_COL32(235, 235, 245, 200), label);
    ImVec2 ts = ImGui::CalcTextSize(name);
    float bw = std::max(64.f, ts.x + 22);
    ImVec2 a(p.x + w - bw, p.y), b(p.x + w, p.y + 26);
    dl->AddRectFilled(a, b, listen ? AccU(0.32f) : IM_COL32(34, 34, 46, 255), 6);
    dl->AddRect(a, b, AccU(listen ? 0.85f : 0.22f), 6);
    dl->AddText(ImVec2(a.x + (bw - ts.x) * 0.5f, p.y + 5), IM_COL32_WHITE, name);
    ImGui::PopID();
}
static bool Button(const char* label, float w = -1) {
    ImGui::PushID(label);
    float aw = w < 0 ? CW() : w;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pr = ImGui::InvisibleButton("##b", ImVec2(aw, 30));
    float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("h"), 0.f);
    hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 14);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + aw, p.y + 30), AccU(0.12f + 0.25f * hv), 7);
    dl->AddRect(p, ImVec2(p.x + aw, p.y + 30), AccU(0.35f + 0.25f * hv), 7);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (aw - ts.x) * 0.5f, p.y + 7), IM_COL32_WHITE, label);
    ImGui::PopID();
    return pr;
}
static bool ChipButton(const char* label, float w) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pr = ImGui::InvisibleButton("##chip", ImVec2(w, 26));
    float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("h"), 0.f);
    hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 16);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 26), IM_COL32(30, 30, 42, 255), 6);
    if (hv > 0.01f) dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 26), AccU(0.22f * hv), 6);
    dl->AddRect(p, ImVec2(p.x + w, p.y + 26), AccU(0.20f + 0.35f * hv), 6);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + 5), IM_COL32(225, 225, 240, 255), label);
    ImGui::PopID();
    return pr;
}
static bool ColorChip(const char* id, const float* c, float size) {
    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pr = ImGui::InvisibleButton("##cc", ImVec2(size, size));
    float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("h"), 0.f);
    hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 16);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 3;
    dl->AddCircleFilled(ImVec2(p.x + size * 0.5f, p.y + size * 0.5f), size * 0.5f - pad,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], 1.0f)), 24);
    dl->AddCircle(ImVec2(p.x + size * 0.5f, p.y + size * 0.5f), size * 0.5f - pad, IM_COL32(255, 255, 255, (int)(50 + 120 * hv)), 24, 1.5f);
    if (hv > 0.01f) dl->AddCircle(ImVec2(p.x + size * 0.5f, p.y + size * 0.5f), size * 0.5f - pad + 3 * hv, AccU(0.5f * hv), 24, 1.2f);
    ImGui::PopID();
    return pr;
}
// ============================================================ прицел
static const char* kStyleNames =
    "Крест\0Круг\0Точка\0T-образный\0X-образный\0Квадрат\0Стрелки\0Ромб\0"
    "Кольцо + точка\0Тактический\0Скобки\0Звезда\0";
static const char* kStyleList[12] = {"Крест", "Круг", "Точка", "T-образный", "X-образный", "Квадрат",
                                     "Стрелки", "Ромб", "Кольцо + точка", "Тактический", "Скобки", "Звезда"};

struct CrossPreset {
    const char* name;
    int style;
    float size, gap, thick, dotSize;
    bool dot, outline, glow, spin, rainbow;
    float col[3];
};
static const CrossPreset kPresets[] = {
    {"Классика", 0, 8, 4, 2.0f, 2.2f, true, true, false, false, false, {1, 1, 1}},
    {"PvP", 0, 6, 3, 2.0f, 1.6f, true, true, true, false, false, {0.40f, 1, 0.50f}},
    {"Минимал", 2, 4, 4, 2.0f, 2.4f, false, false, false, false, false, {1, 1, 1}},
    {"Неон", 8, 10, 5, 2.0f, 2.0f, true, true, true, false, true, {1, 1, 1}},
    {"Тактикал", 3, 9, 4, 2.0f, 2.2f, false, true, false, false, false, {1, 0.45f, 0.35f}},
    {"Кибер", 5, 11, 5, 2.0f, 2.0f, true, true, true, true, false, {0.10f, 0.90f, 1}},
    {"Стрелки", 6, 9, 4, 2.0f, 2.0f, true, true, true, false, false, {0.80f, 0.90f, 1}},
    {"Звезда", 11, 10, 3, 2.0f, 1.8f, true, true, true, true, true, {1, 0.90f, 0.40f}},
};
static const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
static const char* kPresetNames =
    "Свой\0Классика\0PvP\0Минимал\0Неон\0Тактикал\0Кибер\0Стрелки\0Звезда\0";

static void ApplyPreset(int i) {
    if (i < 0 || i >= kPresetCount) return;
    const CrossPreset& p = kPresets[i];
    cfg.crossStyle = p.style;
    cfg.crossSize = p.size;
    cfg.crossGap = p.gap;
    cfg.crossThick = p.thick;
    cfg.crossDotSize = p.dotSize;
    cfg.crossDot = p.dot;
    cfg.crossOutline = p.outline;
    cfg.crossGlow = p.glow;
    cfg.crossSpin = p.spin;
    cfg.crossRainbow = p.rainbow;
    cfg.crossUseAccent = false;
    cfg.crossCol[0] = p.col[0];
    cfg.crossCol[1] = p.col[1];
    cfg.crossCol[2] = p.col[2];
    cfg.crossCol[3] = 1;
    cfg.crossPreset = i;
    g_stylePop = 1;
}

// Рисует прицел в точке c. sc — масштаб (для предпросмотра), extraPulse — дополнительный импульс.
static void DrawCrosshairAt(ImDrawList* dl, ImVec2 c, float sc, float extraPulse = 0.f) {
    if (!cfg.crossOn) return;
    float op = Clamp01(cfg.crossOpacity);
    if (op <= 0.01f) return;

    float pulse = std::max(0.f, g_pulse + extraPulse);
    float pop = Clamp01(g_stylePop);
    float s = cfg.crossSize * sc * (1.f + 0.18f * pop);
    float gap = (cfg.crossGap + pulse * cfg.crossDyn) * sc * (1.f + 0.10f * pop);
    float th = std::max(0.6f, cfg.crossThick * sc);
    float dotR = cfg.crossDotSize * sc;
    float rot = g_spin;
    int style = ImClamp(cfg.crossStyle, 0, 11);

    auto partColor = [&](int k) -> ImU32 {
        float r, g, b;
        float a = op;
        if (cfg.crossRainbow) {
            float h = fmodf((float)ImGui::GetTime() * 0.12f + k * 0.075f, 1.f);
            ImGui::ColorConvertHSVtoRGB(h, 0.70f, 1.f, r, g, b);
        } else if (cfg.crossUseAccent) {
            r = g_acc.x; g = g_acc.y; b = g_acc.z;
        } else {
            r = cfg.crossCol[0]; g = cfg.crossCol[1]; b = cfg.crossCol[2];
            a *= cfg.crossCol[3];
        }
        return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(Clamp01(a) * 255));
    };

    auto shape = [&](ImU32 fixed, bool useParts, float extra) {
        float t = th + extra;
        auto C = [&](int k) -> ImU32 { return useParts ? partColor(k) : fixed; };
        switch (style) {
        case 0: { // крест
            for (int k = 0; k < 4; k++) {
                float a = rot + k * 1.5707964f;
                float dx = cosf(a), dy = sinf(a);
                dl->AddLine(ImVec2(c.x + dx * gap, c.y + dy * gap), ImVec2(c.x + dx * (gap + s), c.y + dy * (gap + s)), C(k), t);
            }
            break;
        }
        case 1: // круг
            dl->AddCircle(c, gap + s * 0.55f, C(0), 48, t);
            break;
        case 2: { // точка
            float r = std::max(1.4f * sc, std::max(dotR, s * 0.38f)) + extra * 0.5f;
            dl->AddCircleFilled(c, r, C(0), 32);
            break;
        }
        case 3: { // T-образный
            for (int k = 0; k < 3; k++) {
                float a = rot + k * 1.5707964f;
                float dx = cosf(a), dy = sinf(a);
                dl->AddLine(ImVec2(c.x + dx * gap, c.y + dy * gap), ImVec2(c.x + dx * (gap + s), c.y + dy * (gap + s)), C(k), t);
            }
            break;
        }
        case 4: { // X-образный
            for (int k = 0; k < 4; k++) {
                float a = rot + 0.7853982f + k * 1.5707964f;
                float dx = cosf(a), dy = sinf(a);
                dl->AddLine(ImVec2(c.x + dx * gap, c.y + dy * gap), ImVec2(c.x + dx * (gap + s), c.y + dy * (gap + s)), C(k), t);
            }
            break;
        }
        case 5: { // квадрат
            float R = gap + s;
            dl->AddRect(ImVec2(c.x - R, c.y - R), ImVec2(c.x + R, c.y + R), C(0), 2 * sc, 0, t);
            break;
        }
        case 6: { // стрелки к центру
            for (int k = 0; k < 4; k++) {
                float a = rot + k * 1.5707964f;
                float dx = cosf(a), dy = sinf(a), px = -dy, py = dx;
                ImVec2 apex(c.x + dx * gap, c.y + dy * gap);
                ImVec2 q1(c.x + dx * (gap + s) + px * s * 0.55f, c.y + dy * (gap + s) + py * s * 0.55f);
                ImVec2 q2(c.x + dx * (gap + s) - px * s * 0.55f, c.y + dy * (gap + s) - py * s * 0.55f);
                dl->AddLine(apex, q1, C(k), t);
                dl->AddLine(apex, q2, C(k), t);
            }
            break;
        }
        case 7: { // ромб
            float R = gap + s;
            ImVec2 pts[4];
            for (int k = 0; k < 4; k++) {
                float a = rot + k * 1.5707964f;
                pts[k] = ImVec2(c.x + cosf(a) * R, c.y + sinf(a) * R);
            }
            for (int k = 0; k < 4; k++) dl->AddLine(pts[k], pts[(k + 1) % 4], C(k), t);
            break;
        }
        case 8: { // кольцо + точка
            float R = gap + s * 0.55f;
            dl->AddCircle(c, R, C(0), 48, t);
            dl->AddCircleFilled(c, std::max(1.4f * sc, dotR), C(1), 20);
            break;
        }
        case 9: { // тактический крест с засечками
            for (int k = 0; k < 4; k++) {
                float a = rot + k * 1.5707964f;
                float dx = cosf(a), dy = sinf(a), px = -dy, py = dx;
                ImVec2 e(c.x + dx * (gap + s), c.y + dy * (gap + s));
                dl->AddLine(ImVec2(c.x + dx * gap, c.y + dy * gap), e, C(k), t);
                float tick = s * 0.30f;
                dl->AddLine(ImVec2(e.x - px * tick - dx * tick, e.y - py * tick - dy * tick),
                            ImVec2(e.x + px * tick - dx * tick, e.y + py * tick - dy * tick), C(k), t);
            }
            break;
        }
        case 10: { // скобки
            float R = gap + s, arm = s * 0.52f;
            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sy = -1; sy <= 1; sy += 2) {
                    ImVec2 corner(c.x + sx * R, c.y + sy * R);
                    dl->AddLine(corner, ImVec2(c.x + sx * (R - arm), c.y + sy * R), C(0), t);
                    dl->AddLine(corner, ImVec2(c.x + sx * R, c.y + sy * (R - arm)), C(0), t);
                }
            }
            break;
        }
        case 11: { // звезда (8 лучей)
            for (int k = 0; k < 8; k++) {
                float a = rot + k * 0.7853982f;
                float len = (k % 2 == 0) ? s : s * 0.55f;
                float dx = cosf(a), dy = sinf(a);
                dl->AddLine(ImVec2(c.x + dx * gap, c.y + dy * gap), ImVec2(c.x + dx * (gap + len), c.y + dy * (gap + len)), C(k), t);
            }
            break;
        }
        default:
            break;
        }
    };

    if (cfg.crossGlow) {
        float* gc = cfg.crossUseAccent ? cfg.accent : cfg.crossGlowCol;
        float ga = Clamp01(cfg.crossGlowStr) * op;
        ImU32 g1 = CU(gc, 0.10f * ga), g2 = CU(gc, 0.20f * ga);
        shape(g1, false, 8.f * sc);
        shape(g2, false, 3.5f * sc);
    }
    if (cfg.crossOutline) shape(IM_COL32(0, 0, 0, (int)(205 * op)), false, 2.4f * sc);
    shape(0, true, 0.f);

    // центральная точка
    if (cfg.crossDot && style != 2 && style != 8) {
        float r = std::max(1.2f * sc, dotR * 0.85f);
        if (cfg.crossOutline) dl->AddCircleFilled(c, r + 1.8f * sc, IM_COL32(0, 0, 0, (int)(200 * op)), 20);
        dl->AddCircleFilled(c, r, partColor(0), 20);
    }
}
// ============================================================ эффекты и HUD
static float g_fpsHist[120] = {};
static int g_fpsIdx = 0;

static void DrawEdgeGlow(ImDrawList* dl, ImVec2 ds, float size, ImU32 c) {
    ImU32 c0 = c & 0x00FFFFFF;
    float t = ds.y * size;
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(ds.x, t), c, c, c0, c0);
    dl->AddRectFilledMultiColor(ImVec2(0, ds.y - t), ImVec2(ds.x, ds.y), c0, c0, c, c);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(t, ds.y), c, c0, c0, c);
    dl->AddRectFilledMultiColor(ImVec2(ds.x - t, 0), ImVec2(ds.x, ds.y), c0, c, c, c0);
}

static void SpawnClick(ImVec2 c) {
    g_pulse = 1;
    g_flash = 1;
    if (cfg.burstOn) {
        for (int i = 0; i < cfg.burstCount; i++) {
            float a = (float)i / std::max(1, cfg.burstCount) * 6.2831f + (rand() % 100) * 0.01f;
            float sp = cfg.burstSpeed * (0.45f + (rand() % 100) * 0.011f);
            g_parts.push_back({c.x, c.y, cosf(a) * sp, sinf(a) * sp, 0, std::max(0.1f, cfg.burstLife)});
        }
    }
    if (cfg.ringOn) g_rings.push_back({0, 0.55f});
}

static void DrawToasts(float dt) {
    if (!cfg.notifOn) { g_toasts.clear(); return; }
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImFont* f = g_font ? g_font : ImGui::GetFont();
    float s = ImClamp(cfg.hudScale, 0.8f, 1.4f);
    float h = 30 * s, gap = 7 * s;
    float y = ds.y - 18 * s - h;
    for (int i = (int)g_toasts.size() - 1; i >= 0; i--) {
        ToastMsg& t = g_toasts[i];
        t.t += dt;
        if (t.t >= t.life) { g_toasts.erase(g_toasts.begin() + i); continue; }
        float a = std::min(1.f, t.t / 0.15f) * std::min(1.f, (t.life - t.t) / 0.4f);
        float slide = (1 - std::min(1.f, t.t / 0.22f)) * 26 * s;
        ImVec2 ts = f->CalcTextSizeA(13 * s, FLT_MAX, 0, t.text.c_str());
        float w = ts.x + 26 * s;
        float x = ds.x - 16 * s - w + slide;
        ImVec2 p(x, y);
        dl->AddRectFilled(p, ImVec2(x + w, y + h), IM_COL32(12, 12, 17, (int)(215 * a)), 6);
        dl->AddRectFilled(p, ImVec2(x + 3, y + h), AccU(0.85f * a), 2);
        dl->AddText(f, 13 * s, ImVec2(x + 13 * s, y + (h - ts.y) * 0.5f), IM_COL32(235, 235, 245, (int)(240 * a)), t.text.c_str());
        y -= h + gap;
    }
}

static void DrawWatermark(ImVec2 ds) {
    if (!cfg.wmOn) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont* f = g_font ? g_font : ImGui::GetFont();
    ImFont* fb = g_fontBig ? g_fontBig : f;
    float s = cfg.hudScale;
    float x = ds.x * cfg.wmX, y = ds.y * cfg.wmY;
    float nameFs = 20 * s, fs = 12.5f * s;
    const char* name = "VISUALS";
    ImVec2 ns = fb->CalcTextSizeA(nameFs, FLT_MAX, 0, name);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char fps[32] = "", clk[32] = "", dt[32] = "";
    if (cfg.wmFps) snprintf(fps, sizeof fps, "%.0f FPS", ImGui::GetIO().Framerate);
    if (cfg.wmClock) snprintf(clk, sizeof clk, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    if (cfg.wmDate) snprintf(dt, sizeof dt, "%02d.%02d.%04d", st.wDay, st.wMonth, st.wYear);

    float padX = 12 * s, h = 36 * s;
    float cx = x + padX + ns.x + 14 * s;
    float fpsW = 0, clkW = 0, dtW = 0;
    if (fps[0]) fpsW = f->CalcTextSizeA(fs, FLT_MAX, 0, fps).x + 14 * s;
    if (clk[0]) clkW = f->CalcTextSizeA(fs, FLT_MAX, 0, clk).x;
    if (dt[0]) dtW = f->CalcTextSizeA(fs, FLT_MAX, 0, dt).x;
    float total = (cx - x) + fpsW + (clk[0] ? clkW + 12 * s : 0) + (dt[0] ? dtW + 12 * s : 0) + padX;

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + total, y + h), IM_COL32(10, 10, 15, 208), 7);
    dl->AddRect(ImVec2(x, y), ImVec2(x + total, y + h), IM_COL32(255, 255, 255, 14), 7);
    float sh = fmodf((float)ImGui::GetTime() * 0.30f, 1.0f);
    float sx = x + sh * total;
    dl->AddRectFilledMultiColor(ImVec2(x, y + h - 2), ImVec2(x + total, y + h), AccU(0.25f), AccU(0.25f), AccU(0.25f), AccU(0.25f));
    dl->AddRectFilledMultiColor(ImVec2(sx, y + h - 2), ImVec2(std::min(x + total, sx + total * 0.3f), y + h),
                                AccU(0.f), AccU(0.9f), AccU(0.9f), AccU(0.f));

    AddTextGradient(dl, fb, nameFs, ImVec2(x + padX, y + (h - ns.y) * 0.5f - 1), name, AccU(), IM_COL32(255, 255, 255, 240));
    dl->AddText(f, 10 * s, ImVec2(x + padX + ns.x - 2, y + h - 15 * s), IM_COL32(150, 150, 175, 200), "v5");

    if (fps[0]) {
        ImVec2 a(cx, y + 7 * s);
        dl->AddRectFilled(a, ImVec2(cx + fpsW, y + h - 7 * s), AccU(0.16f), 5);
        dl->AddText(f, fs, ImVec2(cx + 7 * s, y + (h - fs * 1.35f) * 0.5f), AccU(), fps);
        cx += fpsW + 12 * s;
    }
    if (clk[0]) {
        dl->AddCircleFilled(ImVec2(cx + 2 * s, y + h * 0.5f), 2 * s, AccU(0.7f));
        dl->AddText(f, fs, ImVec2(cx + 8 * s, y + (h - fs * 1.35f) * 0.5f), IM_COL32(225, 225, 240, 235), clk);
        cx += clkW + 20 * s;
    }
    if (dt[0]) dl->AddText(f, fs, ImVec2(cx, y + (h - fs * 1.35f) * 0.5f), IM_COL32(170, 170, 195, 230), dt);
}

static void DrawFpsGraph(ImVec2 ds) {
    if (!cfg.graphOn) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont* f = g_font ? g_font : ImGui::GetFont();
    float s = cfg.hudScale;
    float x = ds.x * cfg.graphX, y = ds.y * cfg.graphY;
    float w = 180 * s, h = 64 * s;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(10, 10, 15, 196), 7);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(255, 255, 255, 12), 7);
    dl->AddText(f, 11 * s, ImVec2(x + 9 * s, y + 6 * s), IM_COL32(150, 150, 175, 220), "FPS");
    char b[32];
    snprintf(b, sizeof b, "%.0f", ImGui::GetIO().Framerate);
    ImVec2 bs = f->CalcTextSizeA(12 * s, FLT_MAX, 0, b);
    dl->AddText(f, 12 * s, ImVec2(x + w - bs.x - 9 * s, y + 5 * s), AccU(), b);

    float gx = x + 9 * s, gy = y + 22 * s, gw = w - 18 * s, gh = h - 30 * s;
    float maxMs = 33.f;
    for (int i = 0; i < 120; i++) maxMs = std::max(maxMs, g_fpsHist[i]);
    maxMs = std::min(maxMs * 1.1f, 80.f);
    dl->AddLine(ImVec2(gx, gy + gh * 0.5f), ImVec2(gx + gw, gy + gh * 0.5f), IM_COL32(255, 255, 255, 10), 1);
    for (int i = 1; i < 120; i++) {
        float a = g_fpsHist[(g_fpsIdx + i - 1) % 120];
        float b2 = g_fpsHist[(g_fpsIdx + i) % 120];
        float x0 = gx + (i - 1) * gw / 119.f, x1 = gx + i * gw / 119.f;
        float y0 = gy + gh * (1.f - ImClamp(1.f - a / maxMs, 0.f, 1.f));
        float y1 = gy + gh * (1.f - ImClamp(1.f - b2 / maxMs, 0.f, 1.f));
        if (cfg.graphFill) dl->AddQuadFilled(ImVec2(x0, gy + gh), ImVec2(x1, gy + gh), ImVec2(x1, y1), ImVec2(x0, y0), AccU(0.10f));
        dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), AccU(0.85f), 1.4f);
    }
}
// ===================== клавиши, список модулей, системная панель
static void KeyBox(ImDrawList* dl, ImVec2 p, ImVec2 sz, const char* label, const char* sub, float press, float s) {
    ImFont* f = g_font ? g_font : ImGui::GetFont();
    float fs = 13 * s;
    dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), IM_COL32(11, 11, 16, 185), 6);
    if (press > 0.01f) {
        dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), AccU(0.55f * press), 6);
        dl->AddRect(ImVec2(p.x - 1, p.y - 1), ImVec2(p.x + sz.x + 1, p.y + sz.y + 1), AccU(0.45f * press), 7, 0, 1.4f);
    }
    dl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), IM_COL32(255, 255, 255, (int)(22 + 40 * press)), 6);
    ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0, label);
    if (sub && sub[0]) {
        ImVec2 ss = f->CalcTextSizeA(fs * 0.72f, FLT_MAX, 0, sub);
        float total = ts.y + ss.y + 2 * s;
        float ty = p.y + (sz.y - total) * 0.5f;
        dl->AddText(f, fs, ImVec2(p.x + (sz.x - ts.x) * 0.5f, ty), IM_COL32(245, 245, 255, 235), label);
        dl->AddText(f, fs * 0.72f, ImVec2(p.x + (sz.x - ss.x) * 0.5f, ty + ts.y + 2 * s), AccU(0.9f), sub);
    } else {
        dl->AddText(f, fs, ImVec2(p.x + (sz.x - ts.x) * 0.5f, p.y + (sz.y - ts.y) * 0.5f), IM_COL32(245, 245, 255, 235), label);
    }
}

static void DrawKeystrokes(ImVec2 ds) {
    if (!cfg.ksOn) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float s = cfg.hudScale;
    float S = 42 * s, G = 4 * s;
    float x = ds.x * cfg.ksX, y = ds.y * cfg.ksY;
    static float ka[12] = {};
    int cpsL = CountClicks(g_clicksL), cpsR = CountClicks(g_clicksR);
    char subL[24], subR[24];
    snprintf(subL, sizeof subL, "%d CPS", cpsL);
    snprintf(subR, sizeof subR, "%d CPS", cpsR);
    auto key = [&](int idx, ImVec2 p, ImVec2 sz, const char* label, const char* sub, int vk) {
        ka[idx] = Approach(ka[idx], KeyDown(vk) ? 1.f : 0.f, 22);
        KeyBox(dl, p, sz, label, sub, ka[idx], s);
    };
    float r0 = y, r1 = y + S + G, r2 = y + 2 * (S + G), r3 = y + 3 * (S + G), r4 = y + 4 * (S + G);
    key(0, ImVec2(x + S + G, r0), ImVec2(S, S), "W", "", 'W');
    key(1, ImVec2(x, r1), ImVec2(S, S), "A", "", 'A');
    key(2, ImVec2(x + S + G, r1), ImVec2(S, S), "S", "", 'S');
    key(3, ImVec2(x + 2 * (S + G), r1), ImVec2(S, S), "D", "", 'D');
    float mw = S * 1.5f + G * 0.5f;
    key(4, ImVec2(x, r2), ImVec2(mw, S), "ЛКМ", cfg.cpsOn ? subL : "", VK_LBUTTON);
    key(5, ImVec2(x + mw + G, r2), ImVec2(mw, S), "ПКМ", cfg.cpsOn ? subR : "", VK_RBUTTON);
    float full = 3 * S + 2 * G;
    if (cfg.ksShift) {
        float hw = (full - G) * 0.5f;
        key(6, ImVec2(x, r3), ImVec2(hw, S * 0.8f), "SHIFT", "", VK_LSHIFT);
        key(7, ImVec2(x + hw + G, r3), ImVec2(hw, S * 0.8f), "CTRL", "", VK_LCONTROL);
        if (cfg.ksSpace) key(8, ImVec2(x, r4), ImVec2(full, S * 0.8f), "SPACE", "", VK_SPACE);
    } else if (cfg.ksSpace) {
        key(8, ImVec2(x, r3), ImVec2(full, S * 0.8f), "SPACE", "", VK_SPACE);
    }
}

static void DrawArrayList(ImVec2 ds) {
    if (!cfg.listOn) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont* f = g_font ? g_font : ImGui::GetFont();
    float s = cfg.hudScale;
    float fs = 13.5f * s;

    std::vector<std::string> mods;
    auto add = [&](const char* n, bool on) { if (on) mods.push_back(n); };
    add("Crosshair", cfg.crossOn);
    add("Click FX", cfg.burstOn);
    add("Shockwave", cfg.ringOn);
    add("Edge Flash", cfg.flashOn);
    add("Bloom", cfg.bloomOn);
    add("Ambient", cfg.dustOn);
    add("Vignette", cfg.vigOn);
    add("Edge Glow", cfg.glowOn);
    add("Tint", cfg.tintOn);
    add("Watermark", cfg.wmOn);
    add("FPS Graph", cfg.graphOn);
    add("Keystrokes", cfg.ksOn);
    add("System", cfg.sysOn);

    for (auto& m : mods) g_modAnim[m] = Approach(g_modAnim[m], 1.f, 13);
    for (auto it = g_modAnim.begin(); it != g_modAnim.end();) {
        bool present = std::find(mods.begin(), mods.end(), it->first) != mods.end();
        if (!present) {
            it->second = Approach(it->second, 0.f, 13);
            if (it->second < 0.01f) { it = g_modAnim.erase(it); continue; }
        }
        ++it;
    }

    std::vector<std::pair<std::string, float>> rows(g_modAnim.begin(), g_modAnim.end());
    std::sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
        return f->CalcTextSizeA(fs, FLT_MAX, 0, a.first.c_str()).x > f->CalcTextSizeA(fs, FLT_MAX, 0, b.first.c_str()).x;
    });

    float right = ds.x * cfg.listX;
    float y = ds.y * cfg.listY;
    float rowH = 21 * s;
    for (auto& r : rows) {
        float a = Clamp01(r.second);
        float tw = f->CalcTextSizeA(fs, FLT_MAX, 0, r.first.c_str()).x;
        float w = tw + 20 * s;
        float slide = (1 - a) * 22 * s;
        float x0 = right - w + slide;
        if (cfg.listBg) dl->AddRectFilled(ImVec2(x0, y), ImVec2(x0 + w, y + rowH - 2), IM_COL32(10, 10, 15, (int)(175 * a)), 4);
        dl->AddRectFilled(ImVec2(x0 + w - 3, y + 2), ImVec2(x0 + w, y + rowH - 4), AccU(0.85f * a), 1);
        dl->AddText(f, fs, ImVec2(x0 + 9 * s, y + (rowH - fs * 1.35f) * 0.5f), IM_COL32(240, 240, 250, (int)(230 * a)), r.first.c_str());
        y += rowH;
    }
}

static void DrawSysInfo(ImVec2 ds) {
    if (!cfg.sysOn) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont* f = g_font ? g_font : ImGui::GetFont();
    float s = cfg.hudScale;
    float x = ds.x * cfg.sysX, y = ds.y * cfg.sysY;
    float w = 174 * s, h = 54 * s;
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc);
    ULONGLONG up = (GetTickCount64() - g_startMs) / 1000;
    char l1[64], l2[64];
    snprintf(l1, sizeof l1, "UPTIME %02llu:%02llu:%02llu", up / 3600, (up / 60) % 60, up % 60);
    snprintf(l2, sizeof l2, "RAM %.0f MB", pmc.WorkingSetSize / 1048576.0);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(10, 10, 15, 196), 7);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(255, 255, 255, 12), 7);
    dl->AddCircleFilled(ImVec2(x + 10 * s, y + 16 * s), 2 * s, AccU(0.9f));
    dl->AddCircleFilled(ImVec2(x + 10 * s, y + 36 * s), 2 * s, AccU(0.5f));
    dl->AddText(f, 12 * s, ImVec2(x + 18 * s, y + 9 * s), IM_COL32(205, 205, 225, 235), l1);
    dl->AddText(f, 12 * s, ImVec2(x + 18 * s, y + 29 * s), IM_COL32(165, 165, 190, 230), l2);
}
// ===================== кадр эффектов
static void DrawEffects(float dt) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 ds = io.DisplaySize;
    if (ds.x < 2 || ds.y < 2) return;
    ImVec2 c(ds.x * 0.5f, ds.y * 0.5f);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    g_pulse = std::max(0.f, g_pulse - dt * 4.f);
    g_flash = std::max(0.f, g_flash - dt * 5.f);
    g_stylePop = std::max(0.f, g_stylePop - dt * 5.f);
    if (cfg.crossSpin) g_spin += dt * cfg.crossSpinSpeed * 0.01745f;

    g_fpsHist[g_fpsIdx] = dt * 1000.f;
    g_fpsIdx = (g_fpsIdx + 1) % 120;

    // В главном меню/паузе курсор не захвачен — HUD и эффекты не рисуем.
    bool showWorld = cfg.enabled && (!cfg.hideInMenus || g_inGame);
    if (!showWorld) {
        g_parts.clear();
        g_rings.clear();
        g_dust.clear();
        g_trail.clear();
    } else {
        if (cfg.tintOn) dl->AddRectFilled(ImVec2(0, 0), ds, CU(cfg.tintCol));
        if (cfg.vigOn) DrawEdgeGlow(dl, ds, cfg.vigSize, CU(cfg.vigCol, cfg.vigStr));
        if (cfg.glowOn) DrawEdgeGlow(dl, ds, cfg.glowSize, CU(cfg.glowCol, cfg.glowStr));
        if (cfg.flashOn && g_flash > 0.01f) DrawEdgeGlow(dl, ds, 0.16f, CU(cfg.flashCol, cfg.flashStr * g_flash));
        if (cfg.bloomOn) {
            float R = ds.y * cfg.bloomSize;
            for (int i = 8; i >= 1; i--)
                dl->AddCircleFilled(c, R * i / 8.f, CU(cfg.bloomCol, 0.022f * cfg.bloomStr), 64);
        }

        if (cfg.dustOn) {
            int target = ImClamp(cfg.dustCount, 0, 200);
            while ((int)g_dust.size() < target) {
                float px = (float)(rand() % 2000) / 2000.f * ds.x;
                float py = (float)(rand() % 2000) / 2000.f * ds.y;
                g_dust.push_back({px, py, ((rand() % 100) / 100.f - 0.5f) * 0.6f,
                                  -(0.3f + (rand() % 100) / 100.f), (float)(rand() % 628) / 100.f,
                                  0.5f + (rand() % 100) / 100.f});
            }
            if ((int)g_dust.size() > target) g_dust.resize(target);
            for (auto& p : g_dust) {
                p.x += p.vx * cfg.dustSpeed * dt;
                p.y += p.vy * cfg.dustSpeed * dt;
                p.ph += dt * 1.5f;
                if (p.y < -12) { p.y = ds.y + 12; p.x = (float)(rand() % 2000) / 2000.f * ds.x; }
                if (p.x < -12) p.x = ds.x + 12;
                if (p.x > ds.x + 12) p.x = -12;
                float a = cfg.dustCol[3] * (0.30f + 0.70f * (0.5f + 0.5f * sinf(p.ph)));
                dl->AddCircleFilled(ImVec2(p.x, p.y), cfg.dustSize * p.sz, CU(cfg.dustCol, a), 8);
            }
        } else {
            g_dust.clear();
        }

        if (cfg.trailOn && g_menuOpen) {
            static ImVec2 last(-1, -1);
            if (fabsf(io.MousePos.x - last.x) > 1 || fabsf(io.MousePos.y - last.y) > 1) {
                g_trail.push_back({io.MousePos, 0});
                last = io.MousePos;
            }
        }
        for (auto& t : g_trail) t.age += dt;
        g_trail.erase(std::remove_if(g_trail.begin(), g_trail.end(), [](auto& t) { return t.age >= cfg.trailLife; }), g_trail.end());
        for (size_t i = 1; i < g_trail.size(); i++) {
            float k = 1 - g_trail[i].age / cfg.trailLife;
            dl->AddLine(g_trail[i - 1].p, g_trail[i].p, CU(cfg.trailCol, k), cfg.trailWidth * k + 0.5f);
        }

        if (cfg.crossOn && !(g_menuOpen && cfg.crossHideMenu)) DrawCrosshairAt(dl, c, 1.f);

        for (auto& p : g_parts) {
            p.age += dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vy += cfg.burstGrav * dt;
        }
        for (size_t i = 0; i < g_parts.size(); i++) {
            Particle& p = g_parts[i];
            float k = 1 - p.age / p.life;
            if (k <= 0) continue;
            ImU32 col = CU(cfg.burstCol, k);
            bool spark = (cfg.burstShape == 1) || (cfg.burstShape == 2 && (i % 2) == 1);
            if (spark) {
                dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x - p.vx * 0.025f, p.y - p.vy * 0.025f), col, std::max(1.f, cfg.burstSize * k * 0.8f));
            } else {
                dl->AddCircleFilled(ImVec2(p.x, p.y), cfg.burstSize * k * 2.2f, CU(cfg.burstCol, k * 0.22f), 10);
                dl->AddCircleFilled(ImVec2(p.x, p.y), cfg.burstSize * k + 0.5f, col, 10);
            }
        }
        g_parts.erase(std::remove_if(g_parts.begin(), g_parts.end(), [](auto& p) { return p.age >= p.life; }), g_parts.end());

        for (auto& r : g_rings) r.age += dt;
        for (auto& r : g_rings) {
            float t = r.age / r.life;
            if (t >= 1) continue;
            float R = cfg.ringSize * cfg.ringSpeed * (1.f - powf(1.f - t, 3.f));
            dl->AddCircle(c, R, CU(cfg.ringCol, (1 - t) * 0.85f), 64, std::max(0.6f, cfg.ringWidth * (1 - t)));
        }
        g_rings.erase(std::remove_if(g_rings.begin(), g_rings.end(), [](auto& r) { return r.age >= r.life; }), g_rings.end());

        DrawWatermark(ds);
        DrawFpsGraph(ds);
        DrawKeystrokes(ds);
        DrawArrayList(ds);
        DrawSysInfo(ds);

        if (g_firstFrame) {
            g_firstFrame = false;
            if (cfg.notifOn) Toast("Visuals Overlay v5 · INSERT — меню");
        }
    }
    if (showWorld || g_menuOpen) DrawToasts(dt);
}
// ============================================================ функции (grid как в PulseVisuals)
struct ModuleEntry {
    const char* name;
    const char* desc;
    bool* value;
    int cat;   // 1 — Visuals, 2 — HUD, 3 — Utilities
};
static const ModuleEntry kModules[] = {
    {"Crosshair", "прицел и пресеты", &cfg.crossOn, 1},
    {"Click FX", "частицы и искры", &cfg.burstOn, 1},
    {"Shockwave", "кольцо от клика", &cfg.ringOn, 1},
    {"Edge Flash", "вспышка краёв", &cfg.flashOn, 1},
    {"Ambient", "пыль в воздухе", &cfg.dustOn, 1},
    {"Cursor Trail", "след в меню", &cfg.trailOn, 1},
    {"Vignette", "затемнение краёв", &cfg.vigOn, 1},
    {"Edge Glow", "свечение краёв", &cfg.glowOn, 1},
    {"Tint", "цветовой фильтр", &cfg.tintOn, 1},
    {"Bloom", "свечение центра", &cfg.bloomOn, 1},
    {"Watermark", "лого, FPS, часы", &cfg.wmOn, 2},
    {"FPS Graph", "история кадров", &cfg.graphOn, 2},
    {"Keystrokes", "WASD и CPS", &cfg.ksOn, 2},
    {"Module List", "активные функции", &cfg.listOn, 2},
    {"System Info", "uptime и RAM", &cfg.sysOn, 2},
    {"Notifications", "уведомления", &cfg.notifOn, 2},
    {"Rainbow", "радужный акцент", &cfg.rainbow, 3},
    {"Window Glow", "свечение окна", &cfg.glow, 3},
    {"Auto Save", "сохранение конфига", &cfg.autoSave, 3},
    {"Hide Outside", "не показывать в меню MC", &cfg.hideInMenus, 3},
    {"Crosshair in GUI", "скрывать прицел в меню", &cfg.crossHideMenu, 3},
};
static const int kModuleCount = (int)(sizeof(kModules) / sizeof(kModules[0]));

static bool CatChip(const char* label, bool active, float w) {
    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pr = ImGui::InvisibleButton("##cat", ImVec2(w, 26));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("h"), 0.f);
    hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 16);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 26), active ? AccU(0.20f + 0.10f * hv) : IM_COL32(24, 24, 33, 255), 7);
    dl->AddRect(p, ImVec2(p.x + w, p.y + 26), active ? AccU(0.55f) : IM_COL32(255, 255, 255, 16 + (int)(22 * hv)), 7);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + 5),
                active ? IM_COL32(245, 245, 255, 255) : IM_COL32(180, 180, 200, 235), label);
    ImGui::PopID();
    return pr;
}

static bool ModuleCard(const char* name, const char* desc, bool* value, float w) {
    ImGui::PushID(name);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pr = ImGui::InvisibleButton("##mod", ImVec2(w, 46));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.f);
    float& pop = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("pop"), 0.f);
    hv = Approach(hv, ImGui::IsItemHovered() ? 1.f : 0.f, 18);
    pop = std::max(0.f, pop - ImGui::GetIO().DeltaTime * 4.f);
    bool on = *value;
    if (pr) {
        *value = !on;
        on = *value;
        pop = 1.f;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = on ? AccU(0.10f + 0.05f * hv + 0.05f * pop) : IM_COL32(24, 24, 34, 235);
    if (!on && hv > 0.01f) bg = IM_COL32(30, 30, 42, 240);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 46), bg, 9);
    dl->AddRect(p, ImVec2(p.x + w, p.y + 46),
                on ? AccU(0.40f + 0.25f * hv) : IM_COL32(255, 255, 255, 14 + (int)(18 * hv)), 9);

    ImVec2 dot(p.x + 16, p.y + 23);
    dl->AddCircleFilled(dot, 6.f + pop * 2.f, on ? AccU(0.16f + 0.20f * pop) : IM_COL32(255, 255, 255, 8), 20);
    dl->AddCircleFilled(dot, 3.2f, on ? AccU() : IM_COL32(110, 110, 130, 255), 16);

    ImFont* f = g_font ? g_font : ImGui::GetFont();
    dl->AddText(f, 13.f, ImVec2(p.x + 30, p.y + 8),
                on ? IM_COL32(240, 240, 250, 255) : IM_COL32(205, 205, 220, 240), name);
    dl->AddText(f, 11.f, ImVec2(p.x + 30, p.y + 26), IM_COL32(130, 130, 155, 210), desc);

    float sw = 26, sh = 14, sx = p.x + w - sw - 12, sy = p.y + 16;
    dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh),
                      on ? AccU(0.85f) : IM_COL32(255, 255, 255, 20), sh * 0.5f);
    dl->AddCircleFilled(ImVec2(on ? sx + sw - sh * 0.5f : sx + sh * 0.5f, sy + sh * 0.5f),
                        sh * 0.5f - 2.5f, IM_COL32(255, 255, 255, 245), 16);
    ImGui::PopID();
    return pr;
}

// ============================================================ меню
static void DrawMenu() {
    static float indY = 100;
    int& tab = g_menuTab;
    float& tabAnim = g_menuTabAnim;
    const char* tabs[] = {"Функции", "Прицел", "Эффекты", "Экран", "HUD", "Тема", "Конфиг"};
    const char* subs[] = {
        "Все модули оверлея — включение одним кликом",
        "12 стилей, пресеты, свечение и реакция на клик",
        "Частицы, ударная волна, вспышка и атмосфера",
        "Виньетка, свечение краёв, тон и bloom",
        "Ватермарк, график FPS, клавиши и список модулей",
        "Акцент, форма окна и анимации",
        "Клавиши, профили и сброс настроек"};
    const int NT = 7;
    const ImVec2 SZ(820, 600);
    float e = 1 - powf(1 - g_menuAnim, 3.f);
    g_menuAlphaNow = cfg.menuAlpha;
    g_previewPulse = std::max(0.f, g_previewPulse - ImGui::GetIO().DeltaTime * 3.f);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_menuAlphaNow);
    ImGui::SetNextWindowPos(ImVec2(g_menuPos.x, g_menuPos.y + (1 - e) * 28), ImGuiCond_Always);
    ImGui::SetNextWindowSize(SZ, ImGuiCond_Always);
    ImGui::Begin("##menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoSavedSettings);
    ImVec2 wp = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float R = cfg.rounding;

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, (int)(255 * cfg.dim * e)));
    for (int i = 1; i <= 10; i++)
        bg->AddRectFilled(ImVec2(wp.x - i * 3.f, wp.y - i * 3.f + 8), ImVec2(wp.x + SZ.x + i * 3.f, wp.y + SZ.y + i * 3.f + 8),
                          IM_COL32(0, 0, 0, (int)(14 * e)), R + i * 3);
    if (cfg.glow) {
        for (int i = 1; i <= 5; i++)
            bg->AddRect(ImVec2(wp.x - i, wp.y - i), ImVec2(wp.x + SZ.x + i, wp.y + SZ.y + i), AccU(0.09f * e / i), R + i, 0, 1.5f);
    }

    dl->AddRectFilled(wp, ImVec2(wp.x + 184, wp.y + SZ.y), IM_COL32(11, 11, 17, 255), R, ImDrawFlags_RoundCornersLeft);
    dl->AddRectFilled(ImVec2(wp.x + 184, wp.y + 14), ImVec2(wp.x + 185, wp.y + SZ.y - 14), IM_COL32(255, 255, 255, 10));
    AddTextGradient(dl, g_fontBig ? g_fontBig : ImGui::GetFont(), 21, ImVec2(wp.x + 22, wp.y + 18), "VISUALS", AccU(), IM_COL32(255, 255, 255, 240));
    dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + 23, wp.y + 46), IM_COL32(120, 120, 145, 220), "OVERLAY v5 · OPENGL");
    dl->AddRectFilled(ImVec2(wp.x + 22, wp.y + 66), ImVec2(wp.x + 88, wp.y + 68), AccU(0.85f), 1);

    float targetY = 100.f + tab * 40.f;
    indY = Approach(indY, targetY, 16);
    dl->AddRectFilled(ImVec2(wp.x + 12, wp.y + indY), ImVec2(wp.x + 172, wp.y + indY + 34), AccU(0.16f), 7);
    dl->AddRectFilled(ImVec2(wp.x + 12, wp.y + indY + 8), ImVec2(wp.x + 15, wp.y + indY + 26), AccU(), 2);
    for (int i = 0; i < NT; i++) {
        ImGui::PushID(i);
        ImGui::SetCursorPos(ImVec2(12, 100.f + i * 40));
        ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##tab", ImVec2(146, 34)) && tab != i) { tab = i; tabAnim = 0; }
        float& hv = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("hv"), 0.f);
        hv = Approach(hv, tab == i ? 1.f : (ImGui::IsItemHovered() ? 0.45f : 0.f), 14);
        dl->AddText(ImGui::GetFont(), 14, ImVec2(p.x + 18 + 3 * hv, p.y + 9), IM_COL32(235, 235, 245, 110 + (int)(140 * hv)), tabs[i]);
        ImGui::PopID();
    }

    // переключатель оверлея
    {
        ImGui::SetCursorPos(ImVec2(12, SZ.y - 92));
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool pr = ImGui::InvisibleButton("##power", ImVec2(146, 32));
        if (pr) {
            cfg.enabled = !cfg.enabled;
            snprintf(g_status, sizeof g_status, cfg.enabled ? "Оверлей включён" : "Оверлей выключен");
        }
        dl->AddRectFilled(p, ImVec2(p.x + 146, p.y + 32), cfg.enabled ? AccU(0.16f) : IM_COL32(255, 255, 255, 8), 7);
        dl->AddCircleFilled(ImVec2(p.x + 16, p.y + 16), 4, cfg.enabled ? AccU() : IM_COL32(120, 120, 135, 255), 16);
        dl->AddText(ImGui::GetFont(), 12, ImVec2(p.x + 28, p.y + 10), cfg.enabled ? IM_COL32(240, 240, 250, 240) : IM_COL32(150, 150, 170, 220),
                    cfg.enabled ? "ОВЕРЛЕЙ ВКЛЮЧЁН" : "ОВЕРЛЕЙ ВЫКЛЮЧЕН");
    }
    {
        char mk[32], tk[32];
        KeyName(cfg.menuKey, mk, sizeof mk);
        KeyName(cfg.toggleKey, tk, sizeof tk);
        char hint[128];
        snprintf(hint, sizeof hint, "%s — меню", mk);
        dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + 18, wp.y + SZ.y - 50), IM_COL32(115, 115, 135, 200), hint);
        snprintf(hint, sizeof hint, "%s — вкл/выкл", tk);
        dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + 18, wp.y + SZ.y - 34), IM_COL32(115, 115, 135, 200), hint);
        dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + 18, wp.y + SZ.y - 18), IM_COL32(115, 115, 135, 200), "1–7 — вкладки");
    }

    dl->AddText(g_fontBig ? g_fontBig : ImGui::GetFont(), 21, ImVec2(wp.x + 206, wp.y + 16), IM_COL32(245, 245, 252, 255), tabs[tab]);
    dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + 208, wp.y + 46), IM_COL32(140, 140, 165, 235), subs[tab]);
    dl->AddRectFilledMultiColor(ImVec2(wp.x + 196, wp.y + 70), ImVec2(wp.x + SZ.x - 16, wp.y + 72),
                                AccU(0.70f), AccU(0.f), AccU(0.f), AccU(0.70f));
    {
        char info[96];
        ULONGLONG up = (GetTickCount64() - g_startMs) / 1000;
        snprintf(info, sizeof info, "%.0f FPS   ·   %02llu:%02llu:%02llu", ImGui::GetIO().Framerate, up / 3600, (up / 60) % 60, up % 60);
        ImVec2 ts = ImGui::CalcTextSize(info);
        dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + SZ.x - 18 - ts.x, wp.y + 26), IM_COL32(130, 130, 155, 220), info);
    }

    tabAnim = Approach(tabAnim, 1, 12);
    ImGui::SetCursorPos(ImVec2(196 + (1 - tabAnim) * 10, 84));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##content", ImVec2(SZ.x - 212, SZ.y - 118), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_menuAlphaNow);

    float contentW = SZ.x - 212;
    float usable = contentW - 12;
    float cw = (usable - 12) * 0.5f;
    float cx1 = 2, cx2 = 2 + cw + 12;

    switch (tab) {
    case 0: {   // функции: сетка модулей как в PulseVisuals
        static int cat = 0;
        int prevCat = cat;
        int cnt[4] = {0, 0, 0, 0};
        for (const ModuleEntry& m : kModules) { cnt[0]++; cnt[m.cat]++; }
        int active = 0;
        for (const ModuleEntry& m : kModules) if (*m.value) active++;

        float y0 = ImGui::GetCursorPosY();
        ImVec2 rowP = ImGui::GetCursorScreenPos();
        ImGui::SetCursorPos(ImVec2(2, y0));
        const char* cats[4] = {"Все", "Visuals", "HUD", "Utilities"};
        for (int i = 0; i < 4; i++) {
            char lbl[48];
            snprintf(lbl, sizeof lbl, "%s  %d", cats[i], cnt[i]);
            float wch = ImGui::CalcTextSize(lbl).x + 26;
            if (i) ImGui::SameLine(0, 8);
            if (CatChip(lbl, cat == i, wch)) cat = i;
        }
        {
            char info[48];
            snprintf(info, sizeof info, "%d / %d активно", active, kModuleCount);
            ImVec2 ts = ImGui::CalcTextSize(info);
            ImGui::GetWindowDrawList()->AddText(ImVec2(rowP.x + contentW - 18 - ts.x, rowP.y + 6),
                                                IM_COL32(130, 130, 155, 220), info);
        }
        if (cat != prevCat) ImGui::SetScrollY(0.f);
        ImGui::SetCursorPos(ImVec2(2, y0 + 38));
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + contentW - 8, p.y + 1), IM_COL32(255, 255, 255, 12));
        }
        ImGui::SetCursorPos(ImVec2(2, y0 + 50));
        int col = 0;
        for (const ModuleEntry& m : kModules) {
            if (cat && m.cat != cat) continue;
            if (col) ImGui::SameLine(0, 12);
            ModuleCard(m.name, m.desc, m.value, cw);
            col = 1 - col;
        }
    } break;
    case 1: {
        float yL = 0, yR = 0;
        BeginCard("Предпросмотр", cx1, yL, cw);
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            float pw = CW(), ph = 132;
            ImGui::InvisibleButton("##preview", ImVec2(pw, ph));
            if (ImGui::IsItemClicked()) { g_previewPulse = 1; }
            ImDrawList* d = ImGui::GetWindowDrawList();
            d->AddRectFilled(p, ImVec2(p.x + pw, p.y + ph), IM_COL32(8, 8, 12, 200), 8);
            for (int i = 1; i < 4; i++) {
                d->AddLine(ImVec2(p.x + pw * i / 4.f, p.y + 8), ImVec2(p.x + pw * i / 4.f, p.y + ph - 8), IM_COL32(255, 255, 255, 8));
                d->AddLine(ImVec2(p.x + 8, p.y + ph * i / 4.f), ImVec2(p.x + pw - 8, p.y + ph * i / 4.f), IM_COL32(255, 255, 255, 8));
            }
            d->AddCircle(ImVec2(p.x + pw * 0.5f, p.y + ph * 0.5f), 24, IM_COL32(255, 255, 255, 10), 32);
            DrawCrosshairAt(d, ImVec2(p.x + pw * 0.5f, p.y + ph * 0.5f), 1.7f, g_previewPulse);
            int si = ImClamp(cfg.crossStyle, 0, 11);
            d->AddText(ImVec2(p.x + 10, p.y + ph - 20), IM_COL32(160, 160, 185, 230), kStyleList[si]);
            const char* tip = "ЛКМ — тест";
            ImVec2 ts = ImGui::CalcTextSize(tip);
            d->AddText(ImVec2(p.x + pw - ts.x - 10, p.y + ph - 20), IM_COL32(120, 120, 145, 200), tip);
        }
        int prevStyle = cfg.crossStyle;
        ComboRow("Стиль", &cfg.crossStyle, kStyleNames);
        if (prevStyle != cfg.crossStyle) { cfg.crossPreset = -1; g_stylePop = 1; }
        int sel = cfg.crossPreset + 1;
        int prevSel = sel;
        ComboRow("Пресет", &sel, kPresetNames);
        if (sel != prevSel) {
            if (sel <= 0) cfg.crossPreset = -1;
            else ApplyPreset(sel - 1);
        }
        yL = EndCard();

        BeginCard("Быстрые пресеты", cx1, yL + 12, cw);
        {
            float bw = (CW() - 8) * 0.5f;
            for (int i = 0; i < kPresetCount; i++) {
                if (i % 2 == 1) ImGui::SameLine(0, 8);
                if (ChipButton(kPresets[i].name, bw)) ApplyPreset(i);
            }
        }
        EndCard();

        BeginCard("Геометрия", cx2, 0, cw);
        SliderF("Размер", &cfg.crossSize, 2, 30, "%.1f");
        SliderF("Зазор", &cfg.crossGap, 0, 24, "%.1f");
        SliderF("Толщина", &cfg.crossThick, 1, 6, "%.1f");
        SliderF("Размер точки", &cfg.crossDotSize, 0, 8, "%.1f");
        SliderF("Прозрачность", &cfg.crossOpacity, 0.2f, 1, "%.2f");
        SliderF("Расширение при клике", &cfg.crossDyn, 0, 24, "%.1f");
        SliderF("Скорость вращения", &cfg.crossSpinSpeed, 10, 360, "%.0f");
        yR = EndCard();

        BeginCard("Оформление", cx2, yR + 12, cw);
        Toggle("Точка в центре", &cfg.crossDot);
        Toggle("Обводка", &cfg.crossOutline);
        Toggle("Вращение", &cfg.crossSpin);
        Toggle("Свечение", &cfg.crossGlow);
        if (cfg.crossGlow) SliderF("Сила свечения", &cfg.crossGlowStr, 0.1f, 1, "%.2f");
        Toggle("Радужный прицел", &cfg.crossRainbow);
        Toggle("Использовать акцент", &cfg.crossUseAccent);
        Toggle("Скрывать в меню", &cfg.crossHideMenu);
        ColorRow("Цвет прицела", cfg.crossCol);
        ColorRow("Цвет свечения", cfg.crossGlowCol);
        EndCard();
        (void)yL;
    } break;
    case 2: {
        float yL = 0, yR = 0;
        BeginCard("Эффект клика", cx1, 0, cw);
        Toggle("Включить", &cfg.burstOn);
        ComboRow("Форма", &cfg.burstShape, "Круги\0Искры\0Смешанный\0");
        SliderI("Количество", &cfg.burstCount, 2, 40);
        SliderF("Скорость", &cfg.burstSpeed, 30, 320, "%.0f");
        SliderF("Время жизни", &cfg.burstLife, 0.2f, 2, "%.2f");
        SliderF("Гравитация", &cfg.burstGrav, 0, 500, "%.0f");
        SliderF("Размер", &cfg.burstSize, 1, 8, "%.1f");
        ColorRow("Цвет", cfg.burstCol);
        yL = EndCard();

        BeginCard("Ударная волна", cx1, yL + 12, cw);
        Toggle("Включить", &cfg.ringOn);
        SliderF("Радиус", &cfg.ringSize, 10, 140, "%.0f");
        SliderF("Толщина", &cfg.ringWidth, 1, 6, "%.1f");
        SliderF("Скорость", &cfg.ringSpeed, 0.4f, 3, "%.2f");
        ColorRow("Цвет", cfg.ringCol);
        EndCard();

        BeginCard("Вспышка краёв", cx2, 0, cw);
        Toggle("Включить", &cfg.flashOn);
        SliderF("Сила", &cfg.flashStr, 0.05f, 1, "%.2f");
        ColorRow("Цвет", cfg.flashCol);
        yR = EndCard();

        BeginCard("Пыль в воздухе", cx2, yR + 12, cw);
        Toggle("Включить", &cfg.dustOn);
        SliderI("Количество", &cfg.dustCount, 10, 160);
        SliderF("Скорость", &cfg.dustSpeed, 4, 40, "%.0f");
        SliderF("Размер", &cfg.dustSize, 0.5f, 4, "%.1f");
        ColorRow("Цвет", cfg.dustCol);
        yR = EndCard();

        BeginCard("След курсора в меню", cx2, yR + 12, cw);
        Toggle("Включить", &cfg.trailOn);
        SliderF("Длина (сек)", &cfg.trailLife, 0.1f, 1.5f, "%.2f");
        SliderF("Ширина", &cfg.trailWidth, 1, 12, "%.0f");
        ColorRow("Цвет", cfg.trailCol);
        EndCard();
        (void)yL;
    } break;
    case 3: {
        float yL = 0, yR = 0;
        BeginCard("Виньетка", cx1, 0, cw);
        Toggle("Включить", &cfg.vigOn);
        SliderF("Размер", &cfg.vigSize, 0.05f, 0.5f, "%.2f");
        SliderF("Сила", &cfg.vigStr, 0.1f, 1, "%.2f");
        ColorRow("Цвет", cfg.vigCol);
        yL = EndCard();

        BeginCard("Свечение краёв", cx1, yL + 12, cw);
        Toggle("Включить", &cfg.glowOn);
        SliderF("Размер", &cfg.glowSize, 0.05f, 0.5f, "%.2f");
        SliderF("Сила", &cfg.glowStr, 0.1f, 1, "%.2f");
        ColorRow("Цвет", cfg.glowCol);
        EndCard();

        BeginCard("Тонирование", cx2, 0, cw);
        Toggle("Включить", &cfg.tintOn);
        ColorRow("Цвет и прозрачность", cfg.tintCol);
        yR = EndCard();

        BeginCard("Свечение центра", cx2, yR + 12, cw);
        Toggle("Включить", &cfg.bloomOn);
        SliderF("Размер", &cfg.bloomSize, 0.03f, 0.35f, "%.2f");
        SliderF("Сила", &cfg.bloomStr, 0.05f, 1, "%.2f");
        ColorRow("Цвет", cfg.bloomCol);
        yR = EndCard();

        BeginCard("Окно и атмосфера", cx2, yR + 12, cw);
        SliderF("Затемнение фона", &cfg.dim, 0, 0.85f, "%.2f");
        Toggle("Свечение окна", &cfg.glow);
        Toggle("Скрывать прицел в меню", &cfg.crossHideMenu);
        EndCard();
    } break;
    case 4: {
        float yL = 0, yR = 0;
        BeginCard("Ватермарк", cx1, 0, cw);
        Toggle("Включить", &cfg.wmOn);
        Toggle("Показывать FPS", &cfg.wmFps);
        Toggle("Показывать часы", &cfg.wmClock);
        Toggle("Показывать дату", &cfg.wmDate);
        SliderF("Масштаб HUD", &cfg.hudScale, 0.6f, 1.6f, "%.2f");
        SliderF("Позиция X", &cfg.wmX, 0, 0.7f, "%.3f");
        SliderF("Позиция Y", &cfg.wmY, 0, 0.6f, "%.3f");
        yL = EndCard();

        BeginCard("График FPS", cx1, yL + 12, cw);
        Toggle("Включить", &cfg.graphOn);
        Toggle("Заливка графика", &cfg.graphFill);
        SliderF("Позиция X", &cfg.graphX, 0, 0.7f, "%.3f");
        SliderF("Позиция Y", &cfg.graphY, 0, 0.7f, "%.3f");
        EndCard();

        BeginCard("Клавиши и CPS", cx2, 0, cw);
        Toggle("Включить", &cfg.ksOn);
        Toggle("Показывать CPS", &cfg.cpsOn);
        Toggle("Клавиша SPACE", &cfg.ksSpace);
        Toggle("Клавиши SHIFT / CTRL", &cfg.ksShift);
        SliderF("Позиция X", &cfg.ksX, 0, 0.7f, "%.3f");
        SliderF("Позиция Y", &cfg.ksY, 0, 0.7f, "%.3f");
        yR = EndCard();

        BeginCard("Список модулей", cx2, yR + 12, cw);
        Toggle("Включить", &cfg.listOn);
        Toggle("Фон строк", &cfg.listBg);
        SliderF("Позиция X", &cfg.listX, 0.5f, 1, "%.3f");
        SliderF("Позиция Y", &cfg.listY, 0, 0.7f, "%.3f");
        yR = EndCard();

        BeginCard("Системная панель", cx2, yR + 12, cw);
        Toggle("Uptime + RAM", &cfg.sysOn);
        Toggle("Уведомления", &cfg.notifOn);
        SliderF("Позиция X", &cfg.sysX, 0, 0.7f, "%.3f");
        SliderF("Позиция Y", &cfg.sysY, 0, 0.7f, "%.3f");
        EndCard();
        (void)yL;
    } break;
    case 5: {
        static const struct { const char* id; float c[3]; } themes[] = {
            {"purple", {0.45f, 0.35f, 1.0f}},
            {"cyan", {0.10f, 0.80f, 0.95f}},
            {"pink", {1.00f, 0.35f, 0.60f}},
            {"green", {0.30f, 0.90f, 0.50f}},
            {"orange", {1.00f, 0.60f, 0.20f}},
            {"red", {1.00f, 0.30f, 0.35f}},
        };
        BeginCard("Акцентный цвет", cx1, 0, cw);
        ColorRow("Свой цвет", cfg.accent);
        Toggle("Радужный акцент", &cfg.rainbow);
        Toggle("Свечение окна", &cfg.glow);
        for (int i = 0; i < 6; i++) {
            if (i) ImGui::SameLine(0, 8);
            if (ColorChip(themes[i].id, themes[i].c, 24)) {
                cfg.accent[0] = themes[i].c[0];
                cfg.accent[1] = themes[i].c[1];
                cfg.accent[2] = themes[i].c[2];
                cfg.accent[3] = 1;
            }
        }
        EndCard();

        BeginCard("Форма и анимация", cx2, 0, cw);
        if (SliderF("Скругление", &cfg.rounding, 0, 18, "%.0f")) ApplyStyle();
        SliderF("Прозрачность меню", &cfg.menuAlpha, 0.5f, 1, "%.2f");
        SliderF("Скорость анимации", &cfg.animSpeed, 2, 14, "%.0f");
        SliderF("Затемнение фона", &cfg.dim, 0, 0.85f, "%.2f");
        float yR = EndCard();

        BeginCard("Подсказка", cx2, yR + 12, cw);
        ImGui::PushTextWrapPos(g_card.x + 14 + CW());
        ImGui::TextWrapped("Цвет акцента используется в меню, HUD, свечении прицела и элементах интерфейса. "
                           "Радужный режим плавно переливает все акцентные элементы.");
        ImGui::PopTextWrapPos();
        EndCard();
    } break;
    case 6: {
        float yL = 0, yR = 0;
        BeginCard("Клавиши", cx1, 0, cw);
        KeyBind("Меню", &cfg.menuKey);
        KeyBind("Вкл/выкл оверлей", &cfg.toggleKey);
        KeyBind("Сменить прицел", &cfg.styleKey);
        KeyBind("Паника (скрыть)", &cfg.panicKey);
        Toggle("Автосохранение", &cfg.autoSave);
        yL = EndCard();

        BeginCard("Профили", cx1, yL + 12, cw);
        for (int i = 0; i < 3; i++) {
            ImGui::PushID(i);
            ImGui::TextDisabled("Слот %d", i + 1);
            ImGui::SameLine(g_card.x + 14 + 64);
            if (ChipButton("Сохранить", 92)) SaveCfg(i + 1);
            ImGui::SameLine(0, 6);
            if (ChipButton("Загрузить", 92)) { LoadCfg(i + 1); ApplyStyle(); }
            ImGui::PopID();
        }
        yL = EndCard();

        BeginCard("Сброс", cx1, yL + 12, cw);
        if (Button("Сбросить все настройки")) {
            cfg = Config();
            ApplyStyle();
            g_stylePop = 1;
            strcpy(g_status, "Настройки сброшены");
        }
        EndCard();

        BeginCard("Файл конфигурации", cx2, 0, cw);
        ImGui::PushTextWrapPos(g_card.x + 14 + CW());
        ImGui::TextWrapped("visuals.cfg и visuals_1..3.cfg сохраняются рядом с DLL.");
        ImGui::TextDisabled("Статус:");
        ImGui::TextWrapped("%s", g_status);
        ImGui::PopTextWrapPos();
        if (Button("Сохранить сейчас")) SaveCfg(0);
        if (Button("Загрузить сейчас")) { LoadCfg(0); ApplyStyle(); }
        yR = EndCard();

        BeginCard("О программе", cx2, yR + 12, cw);
        ImGui::PushTextWrapPos(g_card.x + 14 + CW());
        ImGui::TextWrapped("Visuals Overlay v5 — косметический слой: прицел, эффекты клика, HUD и атмосфера. "
                           "Оверлей не читает данные игры: только отрисовка поверх кадра.");
        ImGui::PopTextWrapPos();
        Toggle("Скрывать прицел в меню", &cfg.crossHideMenu);
        Toggle("Скрывать вне игры", &cfg.hideInMenus);
        EndCard();
        (void)yL;
    } break;
    }

    // мягкое затухание у краёв и свой акцентный скроллбар
    {
        ImVec2 cp2 = ImGui::GetWindowPos();
        ImVec2 cs2 = ImGui::GetWindowSize();
        float scy = ImGui::GetScrollY(), scm = ImGui::GetScrollMaxY();
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        if (scy > 1)
            cdl->AddRectFilledMultiColor(ImVec2(cp2.x, cp2.y), ImVec2(cp2.x + cs2.x, cp2.y + 20),
                                         IM_COL32(14, 14, 19, 255), IM_COL32(14, 14, 19, 255),
                                         IM_COL32(14, 14, 19, 0), IM_COL32(14, 14, 19, 0));
        if (scm - scy > 1)
            cdl->AddRectFilledMultiColor(ImVec2(cp2.x, cp2.y + cs2.y - 20), ImVec2(cp2.x + cs2.x, cp2.y + cs2.y),
                                         IM_COL32(14, 14, 19, 0), IM_COL32(14, 14, 19, 0),
                                         IM_COL32(14, 14, 19, 255), IM_COL32(14, 14, 19, 255));
        if (scm > 1) {
            float bx = cp2.x + cs2.x - 6, by = cp2.y + 6, bh = cs2.y - 12;
            float gh = ImMax(28.f, bh * (cs2.y / (cs2.y + scm)));
            float gy = by + (bh - gh) * ImClamp(scy / scm, 0.f, 1.f);
            cdl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + 4, by + bh), IM_COL32(255, 255, 255, 10), 2);
            cdl->AddRectFilled(ImVec2(bx, gy), ImVec2(bx + 4, gy + gh), AccU(0.60f), 2);
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();

    dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + 196, wp.y + SZ.y - 30), IM_COL32(120, 120, 145, 220), g_status);
    {
        char foot[48];
        snprintf(foot, sizeof foot, "Esc — закрыть");
        ImVec2 ts = ImGui::CalcTextSize(foot);
        dl->AddText(ImGui::GetFont(), 11, ImVec2(wp.x + SZ.x - 18 - ts.x, wp.y + SZ.y - 30), IM_COL32(120, 120, 145, 220), foot);
    }
}
// ============================================================ ввод и хук
typedef BOOL(WINAPI* SwapBuffers_t)(HDC);
static SwapBuffers_t oSwap = nullptr;
static WNDPROC oWndProc = nullptr;
static bool g_ready = false;
static volatile LONG g_installed = 0;
static LARGE_INTEGER g_freq{}, g_last{};
typedef void(APIENTRY* glBindFramebuffer_t)(unsigned int, unsigned int);
static glBindFramebuffer_t pBindFramebuffer = nullptr;
static const unsigned GL_FRAMEBUFFER_ = 0x8D40, GL_FRAMEBUFFER_BINDING_ = 0x8CA6;

static void ClientSize(int& w, int& h) {
    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
}

// Игра в мире просит GLFW скрыть курсор и зажать его в окне. В любом экране курсор обычный.
static bool CursorCaptured() {
    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    if (GetCursorInfo(&ci)) {
        if (ci.hCursor == nullptr || !(ci.flags & CURSOR_SHOWING)) return true;
    }
    RECT clip{}, client{};
    if (GetClipCursor(&clip) && GetClientRect(g_hwnd, &client)) {
        POINT tl{client.left, client.top}, br{client.right, client.bottom};
        ClientToScreen(g_hwnd, &tl);
        ClientToScreen(g_hwnd, &br);
        bool matches = std::abs(clip.left - tl.x) <= 2 && std::abs(clip.top - tl.y) <= 2 &&
                       std::abs(clip.right - br.x) <= 2 && std::abs(clip.bottom - br.y) <= 2;
        bool whole = (clip.right - clip.left) >= GetSystemMetrics(SM_CXVIRTUALSCREEN) &&
                     (clip.bottom - clip.top) >= GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (matches && !whole) return true;
    }
    return false;
}
static void ClampCursor() {
    int w, h;
    ClientSize(w, h);
    g_vx = std::fmin((float)(w - 1), std::fmax(0.f, g_vx));
    g_vy = std::fmin((float)(h - 1), std::fmax(0.f, g_vy));
}
static void OpenMenu() {
    int w, h;
    ClientSize(w, h);
    g_vx = w / 2.f;
    g_vy = h / 2.f;
    g_menuOpen = true;
    float sc = g_scale;
    g_menuPos = ImVec2((w / sc - 820) * 0.5f, (h / sc - 600) * 0.5f);
}
static void CloseMenu() {
    if (g_menuOpen && cfg.autoSave) SaveCfg(0, true);
    g_menuOpen = false;
    g_bindTarget = nullptr;
}

static LRESULT CALLBACK hkWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    ImGuiIO& io = ImGui::GetIO();
    if (g_bindTarget && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)) {
        *g_bindTarget = (wp == VK_ESCAPE) ? 0 : (int)wp;
        g_bindTarget = nullptr;
        return 0;
    }
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !(lp & (1 << 30))) {
        int k = (int)wp;
        if (k == cfg.menuKey) {
            if (g_menuOpen) { CloseMenu(); return 0; }
            if (GetForegroundWindow() == g_hwnd) { OpenMenu(); return 0; }
        } else if (!g_menuOpen && k != 0 && k == cfg.toggleKey) {
            cfg.enabled = !cfg.enabled;
            Toast("Оверлей %s", cfg.enabled ? "включён" : "выключен");
            return 0;
        } else if (!g_menuOpen && k != 0 && k == cfg.styleKey) {
            cfg.crossStyle = (ImClamp(cfg.crossStyle, 0, 11) + 1) % 12;
            cfg.crossPreset = -1;
            g_stylePop = 1;
            Toast("Прицел: %s", kStyleList[cfg.crossStyle]);
            return 0;
        } else if (k != 0 && k == cfg.panicKey) {
            CloseMenu();
            cfg.enabled = false;
            Toast("Паника: оверлей скрыт");
            return 0;
        }
    }
    if (!g_menuOpen) {
        if (msg == WM_LBUTTONDOWN) {
            g_clicksL.push_back(GetTickCount64() / 1000.0);
            if (g_captured) {
                int w, hh;
                ClientSize(w, hh);
                SpawnClick(ImVec2(w / g_scale * 0.5f, hh / g_scale * 0.5f));
            }
        } else if (msg == WM_RBUTTONDOWN) {
            g_clicksR.push_back(GetTickCount64() / 1000.0);
        }
        return CallWindowProcW(oWndProc, h, msg, wp, lp);
    }
    switch (msg) {   // меню открыто: игра ввода не получает
    case WM_INPUT: {
        RAWINPUT ri;
        UINT size = sizeof(ri);
        if (g_captured && GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
            g_vx += (float)ri.data.mouse.lLastX;
            g_vy += (float)ri.data.mouse.lLastY;
            ClampCursor();
            if (g_scale > 0.01f) io.AddMousePosEvent(g_vx / g_scale, g_vy / g_scale);
        }
        return DefWindowProcW(h, msg, wp, lp);
    }
    case WM_MOUSEMOVE:
        if (!g_captured) {
            g_vx = (float)(short)(lp & 0xFFFF);
            g_vy = (float)(short)((lp >> 16) & 0xFFFF);
            ClampCursor();
            if (g_scale > 0.01f) io.AddMousePosEvent(g_vx / g_scale, g_vy / g_scale);
        }
        return 0;
    case WM_LBUTTONDOWN:
        io.AddMouseButtonEvent(0, true);
        return 0;
    case WM_LBUTTONUP:
        io.AddMouseButtonEvent(0, false);
        return 0;
    case WM_RBUTTONDOWN:
        io.AddMouseButtonEvent(1, true);
        return 0;
    case WM_RBUTTONUP:
        io.AddMouseButtonEvent(1, false);
        return 0;
    case WM_MBUTTONDOWN:
        io.AddMouseButtonEvent(2, true);
        return 0;
    case WM_MBUTTONUP:
        io.AddMouseButtonEvent(2, false);
        return 0;
    case WM_MOUSEWHEEL:
        io.AddMouseWheelEvent(0.f, GET_WHEEL_DELTA_WPARAM(wp) / 120.f);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp == VK_ESCAPE) CloseMenu();
        else if (wp >= '1' && wp <= '7') { g_menuTab = (int)wp - '1'; g_menuTabAnim = 0; }
        return 0;
    case WM_CHAR:
    case WM_SYSCHAR:
        return 0;
    case WM_KILLFOCUS:
        CloseMenu();
        break;
    default:
        break;
    }
    return CallWindowProcW(oWndProc, h, msg, wp, lp);
}

static void InitGui(HWND hwnd) {
    g_hwnd = hwnd;
    g_startMs = GetTickCount64();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.MouseDrawCursor = false;

    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
    builder.AddText("—–•…·→✓");
    builder.BuildRanges(&ranges);
    ImFontConfig fc;
    fc.OversampleH = 2;
    fc.OversampleV = 1;
    g_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.f, &fc, ranges.Data);
    ImFontConfig fc2 = fc;
    g_fontBig = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 22.f, &fc2, ranges.Data);
    ImFontConfig fc3 = fc;
    g_fontSmall = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 12.f, &fc3, ranges.Data);
    if (!g_font) io.Fonts->AddFontDefault();
    ImGui_ImplOpenGL3_Init("#version 150");
    ImGui_ImplOpenGL3_NewFrame();   // создаём текстуру шрифта сразу, чтобы меню не подвисало при первом открытии

    LoadCfg(0, true);
    ApplyStyle();
    UpdateAccent();
    oWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_last);
    pBindFramebuffer = (glBindFramebuffer_t)wglGetProcAddress("glBindFramebuffer");
    g_ready = true;
}

static void DrawFrame() {
    ImGuiIO& io = ImGui::GetIO();
    int w, h;
    ClientSize(w, h);
    if (w <= 0 || h <= 0) return;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)((double)(now.QuadPart - g_last.QuadPart) / (double)g_freq.QuadPart);
    g_last = now;
    dt = std::fmin(0.1f, std::fmax(0.0001f, dt));

    bool focused = GetForegroundWindow() == g_hwnd;
    g_captured = CursorCaptured();
    g_inGame = focused && g_captured;
    if (g_menuOpen && !focused) CloseMenu();

    g_scale = std::min(1.f, std::min((float)h / 640.f, (float)w / 860.f));
    static int lastW = 0, lastH = 0;
    if (g_menuOpen && (w != lastW || h != lastH))
        g_menuPos = ImVec2((w / g_scale - 820) * 0.5f, (h / g_scale - 600) * 0.5f);
    lastW = w;
    lastH = h;
    io.DeltaTime = dt;
    io.DisplaySize = ImVec2(w / g_scale, h / g_scale);
    io.DisplayFramebufferScale = ImVec2(g_scale, g_scale);
    if (g_menuAnim > 0.001f) io.AddMousePosEvent(g_vx / g_scale, g_vy / g_scale);
    else io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);

    int prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_, &prevFbo);
    if (pBindFramebuffer) pBindFramebuffer(GL_FRAMEBUFFER_, 0);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    UpdateAccent();
    g_menuAnim = ImClamp(g_menuAnim + (g_menuOpen ? 1.f : -1.f) * dt * cfg.animSpeed, 0.f, 1.f);
    DrawEffects(dt);
    if (g_menuAnim > 0.001f) {
        DrawMenu();
        if (g_captured) {   // свой курсор, т.к. системный в игре скрыт
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            ImVec2 p(g_vx / g_scale, g_vy / g_scale);
            fg->AddTriangleFilled(p, ImVec2(p.x + 12, p.y + 14), ImVec2(p.x + 2, p.y + 18), IM_COL32_WHITE);
            fg->AddTriangle(p, ImVec2(p.x + 12, p.y + 14), ImVec2(p.x + 2, p.y + 18), IM_COL32(0, 0, 0, 255), 1.2f);
        }
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (pBindFramebuffer) pBindFramebuffer(GL_FRAMEBUFFER_, (unsigned)prevFbo);
}

static BOOL WINAPI hkSwapBuffers(HDC dc) {
    HWND w = WindowFromDC(dc);
    if (!g_ready && w) {
        char cls[64] = {};
        GetClassNameA(w, cls, sizeof cls);
        if (std::strncmp(cls, "GLFW", 4) == 0) InitGui(w);
    }
    if (g_ready && w == g_hwnd) DrawFrame();
    return oSwap(dc);
}

static void Install() {
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) return;
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Install, &self);
    wchar_t buf[MAX_PATH] = {};
    if (GetModuleFileNameW(self, buf, MAX_PATH)) {
        std::wstring p(buf);
        size_t s = p.find_last_of(L"\\/");
        g_dir = s == std::wstring::npos ? L"." : p.substr(0, s);
    }
    if (MH_Initialize() == MH_OK) {
        void* target = (void*)GetProcAddress(LoadLibraryA("gdi32.dll"), "SwapBuffers");
        if (target && MH_CreateHook(target, (void*)&hkSwapBuffers, (void**)&oSwap) == MH_OK) MH_EnableHook(target);
    }
}

static DWORD WINAPI InstallThread(LPVOID) {
    Install();
    return 0;
}

extern "C" __declspec(dllexport) int JNI_OnLoad(void*, void*) { Install(); return 0x00010008; }
extern "C" __declspec(dllexport) int Agent_OnLoad(void*, char*, void*) { Install(); return 0; }

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr);
    }
    return TRUE;
}







