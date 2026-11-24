#include "gfx/renderer.h"

#include <GL/gl.h>

#include <cfloat>
#include <cstring>
#include <string>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"
#include "core/config.h"
#include "util/anim.h"
#include "util/log.h"

namespace uv::renderer {
namespace {

// GL 1.1 headers shipped with the Windows SDK do not declare these.
constexpr GLenum kFramebuffer = 0x8D40;
constexpr GLenum kFramebufferBinding = 0x8CA6;

// Fonts are rasterised at 2x and scaled back down for layout. This keeps text
// sharp for every UI scale up to 2.0 (the limit of the settings slider).
constexpr float kFontBake = 2.f;

HWND g_window = nullptr;
bool g_ready = false;
float g_scale = 1.f;
float g_dpi = 1.f;
ImVec2 g_display = ImVec2(0, 0);
ImVec2 g_clientPx = ImVec2(0, 0);

ImFont* g_fontSmall = nullptr;
ImFont* g_font = nullptr;
ImFont* g_fontMedium = nullptr;
ImFont* g_fontBig = nullptr;
ImFont* g_fontTitle = nullptr;

using PFNGLBINDFRAMEBUFFER = void(APIENTRY*)(GLenum, GLuint);
PFNGLBINDFRAMEBUFFER g_bindFramebuffer = nullptr;
GLint g_prevFbo = 0;

std::wstring ModuleDir() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&ModuleDir), &module))
        return L".";
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return L".";
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return L".";
    *slash = 0;
    return path;
}

std::string Narrow(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out((size_t)size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, out.data(), size, nullptr, nullptr);
    out.pop_back();
    return out;
}

bool FileExists(const std::wstring& path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Looks for a font next to the DLL first (user can drop Inter/Montserrat there),
// then falls back to a Windows system font.
std::string ResolveFont(const char* localFile, const char* systemFile) {
    const std::wstring local = ModuleDir() + L"\\" + std::wstring(localFile, localFile + std::strlen(localFile));
    if (FileExists(local)) return Narrow(local);

    wchar_t winDir[MAX_PATH] = {};
    if (GetWindowsDirectoryW(winDir, MAX_PATH)) {
        const std::wstring system = std::wstring(winDir) + L"\\Fonts\\" +
                                    std::wstring(systemFile, systemFile + std::strlen(systemFile));
        if (FileExists(system)) return Narrow(system);
    }
    return {};
}

const ImWchar* GlyphRanges() {
    // Latin, Cyrillic and the punctuation we actually use (em dash, bullets, quotes).
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,
        0x0400, 0x045F,
        0x2010, 0x203A,
        0x2116, 0x2116,
        0,
    };
    return ranges;
}

ImFont* LoadFont(const char* localRegular, const char* systemRegular, float size) {
    const std::string path = ResolveFont(localRegular, systemRegular);
    if (path.empty()) return nullptr;
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = false;
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size, &config, GlyphRanges());
}

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    g_fontSmall = LoadFont("Inter-Regular.ttf", "segoeui.ttf", 12.f * kFontBake);
    g_font = LoadFont("Inter-Regular.ttf", "segoeui.ttf", 15.f * kFontBake);
    g_fontMedium = LoadFont("Inter-Medium.ttf", "segoeui.ttf", 16.f * kFontBake);
    g_fontBig = LoadFont("Inter-SemiBold.ttf", "seguisb.ttf", 20.f * kFontBake);
    g_fontTitle = LoadFont("Inter-Bold.ttf", "segoeuib.ttf", 26.f * kFontBake);
    io.FontGlobalScale = 1.f / kFontBake;

    // Guarantee that every slot is usable even on a stripped Windows install.
    ImFont* fallback = nullptr;
    if (!g_fontSmall || !g_font || !g_fontMedium || !g_fontBig || !g_fontTitle) {
        fallback = io.Fonts->AddFontDefault();
        Log("fonts: falling back to ImGui default font");
    }
    if (!g_fontSmall) g_fontSmall = fallback;
    if (!g_font) g_font = fallback;
    if (!g_fontMedium) g_fontMedium = fallback;
    if (!g_fontBig) g_fontBig = fallback;
    if (!g_fontTitle) g_fontTitle = fallback;
}

void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(10, 6);
    s.CellPadding = ImVec2(4, 4);
    s.ItemSpacing = ImVec2(8, 8);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.IndentSpacing = 16.f;
    s.ScrollbarSize = 7.f;
    s.GrabMinSize = 10.f;

    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 0.f;
    s.PopupBorderSize = 1.f;
    s.FrameBorderSize = 0.f;

    s.WindowRounding = 16.f;
    s.ChildRounding = 12.f;
    s.FrameRounding = 8.f;
    s.PopupRounding = 12.f;
    s.ScrollbarRounding = 6.f;
    s.GrabRounding = 6.f;

    s.AntiAliasedLines = true;
    s.AntiAliasedFill = true;
    s.AntiAliasedLinesUseTex = true;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.055f, 0.078f, 1.f);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImVec4(0.075f, 0.075f, 0.105f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.07f);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.13f, 0.175f, 1.f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.17f, 0.23f, 1.f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.27f, 1.f);

    c[ImGuiCol_Header] = ImVec4(0.18f, 0.17f, 0.26f, 1.f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.22f, 0.35f, 1.f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.25f, 0.42f, 1.f);

    c[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.20f, 1.f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.21f, 0.20f, 0.29f, 1.f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.24f, 0.37f, 1.f);

    c[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.96f, 1.f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.52f, 0.62f, 1.f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.49f, 0.36f, 1.f, 0.35f);

    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.28f, 0.38f, 0.9f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.38f, 0.50f, 1.f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.46f, 0.40f, 0.70f, 1.f);

    c[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.49f, 0.36f, 1.f, 0.5f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.49f, 0.36f, 1.f, 0.8f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_CheckMark] = ImVec4(0.49f, 0.36f, 1.f, 1.f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.49f, 0.36f, 1.f, 1.f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.62f, 0.52f, 1.f, 1.f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.49f, 0.36f, 1.f, 0.6f);
}

}  // namespace

bool Init(HWND window) {
    if (g_ready) return true;
    g_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyStyle();
    LoadFonts();

    if (!ImGui_ImplWin32_InitForOpenGL(window)) {
        Log("renderer: ImGui_ImplWin32_InitForOpenGL failed");
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        Log("renderer: ImGui_ImplOpenGL3_Init failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    g_dpi = Clamp(ImGui_ImplWin32_GetDpiScaleForHwnd(window), 1.f, 2.f);
    g_bindFramebuffer = (PFNGLBINDFRAMEBUFFER)wglGetProcAddress("glBindFramebuffer");
    g_ready = true;
    Log("renderer: initialised (hwnd=%p, dpi=%.2f)", (void*)window, g_dpi);
    return true;
}

void Shutdown() {
    if (!g_ready) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_ready = false;
    g_window = nullptr;
    Log("renderer: shut down");
}

bool Ready() { return g_ready; }
HWND Window() { return g_window; }
float Scale() { return g_scale; }
float DpiScale() { return g_dpi; }
ImVec2 DisplaySize() { return g_display; }

ImVec2 ClientSizePx() {
    if (g_window) {
        RECT rc{};
        if (GetClientRect(g_window, &rc))
            g_clientPx = ImVec2((float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
    }
    return g_clientPx;
}

ImFont* FontSmall() { return g_fontSmall; }
ImFont* Font() { return g_font; }
ImFont* FontMedium() { return g_fontMedium; }
ImFont* FontBig() { return g_fontBig; }
ImFont* FontTitle() { return g_fontTitle; }

void PreFrame(float dt) {
    if (!g_ready) return;

    RECT rc{};
    GetClientRect(g_window, &rc);
    const float width = (float)(rc.right - rc.left);
    const float height = (float)(rc.bottom - rc.top);
    if (width < 8.f || height < 8.f) return;

    g_clientPx = ImVec2(width, height);
    g_scale = Clamp(g_cfg.uiScale, 0.75f, 2.f);
    g_display = ImVec2(width / g_scale, height / g_scale);

    glGetIntegerv(kFramebufferBinding, &g_prevFbo);
    if (g_bindFramebuffer) g_bindFramebuffer(kFramebuffer, 0);

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    io.DisplaySize = g_display;
    io.DisplayFramebufferScale = ImVec2(g_scale, g_scale);
    io.DeltaTime = dt;
}

void EndFrame() {
    if (!g_ready) return;
    ImGui::Render();    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (g_bindFramebuffer) g_bindFramebuffer(kFramebuffer, (GLuint)g_prevFbo);
}

void Shadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha, float spread) {
    const int steps = 12;
    for (int i = 1; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        const float grow = spread * t;
        dl->AddRectFilled(ImVec2(a.x - grow, a.y - grow + 3.f), ImVec2(b.x + grow, b.y + grow + 3.f),
                          IM_COL32(0, 0, 0, (int)(alpha * 18.f * (1.f - t * 0.7f))), rounding + grow);
    }
}

void Glow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, ImU32 color, float spread) {
    const int steps = 8;
    for (int i = steps; i >= 1; --i) {
        const float grow = spread * ((float)i / (float)steps);
        dl->AddRect(ImVec2(a.x - grow, a.y - grow), ImVec2(b.x + grow, b.y + grow),
                    Alpha(color, 0.10f * (1.f - (float)i / (float)steps) + 0.02f), rounding + grow, 0, 1.5f);
    }
}

void GlassPanel(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha) {
    const ImU32 bottom = IM_COL32(15, 15, 22, (int)(240 * alpha));
    dl->AddRectFilled(a, b, bottom, rounding);
    dl->AddRectFilled(ImVec2(a.x + 1.f, a.y + 1.f), ImVec2(b.x - 1.f, a.y + (b.y - a.y) * 0.55f),
                      IM_COL32(255, 255, 255, (int)(7 * alpha)), std::max(0.f, rounding - 1.f));
    dl->AddRect(a, b, IM_COL32(255, 255, 255, (int)(20 * alpha)), rounding, 0, 1.f);
    dl->AddLine(ImVec2(a.x + rounding, a.y + 1.f), ImVec2(b.x - rounding, a.y + 1.f),
                IM_COL32(255, 255, 255, (int)(14 * alpha)));
}

void GradientText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text, ImU32 from, ImU32 to) {
    if (!font || !text) return;
    const int length = (int)std::strlen(text);
    if (length <= 0) return;
    float x = pos.x;
    for (int i = 0; i < length; ++i) {
        const char ch[2] = {text[i], 0};
        const float t = length > 1 ? (float)i / (float)(length - 1) : 0.f;
        dl->AddText(font, size, ImVec2(x, pos.y), Mix(from, to, t), ch);
        x += font->CalcTextSizeA(size, FLT_MAX, 0.f, ch).x;
    }
}

void TextShadow(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text, ImU32 color, float alpha) {
    dl->AddText(font, size, ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, (int)(150 * alpha)), text);
    dl->AddText(font, size, pos, color, text);
}

void SoftCircle(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, int layers) {
    for (int i = layers; i >= 1; --i) {
        const float r = radius * (1.f + 0.55f * ((float)i / (float)layers));
        dl->AddCircleFilled(center, r, Alpha(color, 0.05f + 0.10f / (float)i), 32);
    }
    dl->AddCircleFilled(center, radius, color, 32);
}

}  // namespace uv::renderer
