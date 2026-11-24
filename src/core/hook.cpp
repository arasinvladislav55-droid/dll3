#include "core/hook.h"

#include <windowsx.h>

#include <MinHook.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "backends/imgui_impl_win32.h"
#include "core/config.h"
#include "core/overlay.h"
#include "gfx/gui.h"
#include "gfx/renderer.h"
#include "util/anim.h"
#include "util/log.h"

// The Win32 backend intentionally hides this declaration in its header; the
// examples copy it into their own translation unit.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace uv::hook {
namespace {

using SwapBuffersFn = BOOL(WINAPI*)(HDC);

SwapBuffersFn g_originalWgl = nullptr;
SwapBuffersFn g_originalGdi = nullptr;
WNDPROC g_originalWndProc = nullptr;
HWND g_window = nullptr;
volatile LONG g_installed = 0;
volatile LONG g_wndProcInstalled = 0;
bool g_minHookReady = false;

int g_cursorBoost = 0;
float g_cursorX = 0.f;
float g_cursorY = 0.f;

// Prevents a double render when wglSwapBuffers internally forwards to the GDI
// SwapBuffers export.
thread_local int g_depth = 0;

void ClientSizePx(int& w, int& h) {
    RECT rc{};
    w = h = 0;
    if (g_window) GetClientRect(g_window, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
}

void ClampCursor() {
    int w = 0, h = 0;
    ClientSizePx(w, h);
    if (w <= 0 || h <= 0) return;
    g_cursorX = Clamp(g_cursorX, 0.f, (float)(w - 1));
    g_cursorY = Clamp(g_cursorY, 0.f, (float)(h - 1));
}

bool IsGameWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    char cls[64] = {};
    if (GetClassNameA(hwnd, cls, sizeof cls)) {
        if (std::strstr(cls, "GLFW") || std::strstr(cls, "LWJGL") || std::strstr(cls, "GLFW30"))
            return true;
    }
    return false;
}

bool IsMenuKey(int vk) { return vk != 0 && vk == g_cfg.keyMenu; }
bool IsToggleKey(int vk) { return vk != 0 && vk == g_cfg.keyToggle; }

std::string KeyName(int vk) {
    if (vk <= 0) return "unbound";
    UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC) << 16;
    char buffer[64] = {};
    if (GetKeyNameTextA((LONG)scan, buffer, sizeof buffer) > 0) return buffer;
    std::snprintf(buffer, sizeof buffer, "VK %d", vk);
    return buffer;
}

void OpenMenu() {
    int w = 0, h = 0;
    ClientSizePx(w, h);
    g_cursorX = w * 0.5f;
    g_cursorY = h * 0.5f;
    overlay::SetMenuOpen(true);
}

void CloseMenu() {
    overlay::SetMenuOpen(false);
}

// GLFW hides the cursor with ShowCursor(FALSE) when the player enters a world.
// The ClickGUI needs a visible pointer, so lift the counter while it is open and
// restore it exactly on close.
void UpdateCursorVisibility() {
    const bool open = overlay::MenuOpen();
    if (open && CursorCaptured()) {
        int lifted = 0;
        while (ShowCursor(TRUE) < 0 && lifted < 64) ++lifted;
        g_cursorBoost += lifted;
        if (g_cursorBoost > 64) g_cursorBoost = 64;
        ClipCursor(nullptr);
    } else if (!open && g_cursorBoost > 0) {
        for (int i = 0; i < g_cursorBoost; ++i) ShowCursor(FALSE);
        g_cursorBoost = 0;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ImGuiIO& io = ImGui::GetIO();

    // ------------------------------------------------------- keybind capture
    if (int* target = overlay::BindingTarget()) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            const int vk = (int)wp;
            if (vk == VK_ESCAPE) {
                overlay::CancelKeybind();
                overlay::Toast("Keybind cancelled");
            } else {
                *target = vk;
                overlay::CancelKeybind();
                char text[96];
                std::snprintf(text, sizeof text, "Bound to %s", KeyName(vk).c_str());
                overlay::Toast(text);
            }
            return 0;
        }
        if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) {
            return 0;
        }
    }

    // ----------------------------------------------------------- global keys
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        const bool repeat = (lp & (1 << 30)) != 0;
        const int vk = (int)wp;
        if (!repeat) {
            Log("wndproc: key %d menu=%d fg=%d", vk, overlay::MenuOpen() ? 1 : 0,
                GetForegroundWindow() == hwnd ? 1 : 0);
            if (IsMenuKey(vk)) {
                if (overlay::MenuOpen()) {
                    CloseMenu();
                } else if (GetForegroundWindow() == hwnd) {
                    OpenMenu();
                }
                return 0;
            }
            if (IsToggleKey(vk)) {
                g_cfg.overlayEnabled = !g_cfg.overlayEnabled;
                overlay::Toast(g_cfg.overlayEnabled ? "Visuals enabled" : "Visuals disabled");
                return 0;
            }
            if (vk == VK_ESCAPE && overlay::MenuOpen()) {
                overlay::OnEscape();
                return 0;
            }
            if (overlay::MenuOpen() && vk >= '1' && vk <= '5') {
                gui::SetTab(vk - '1');
                return 0;
            }
        }
    }

    // -------------------------------------------------------- menu is closed
    if (!overlay::MenuOpen()) {
        switch (msg) {
            case WM_MOUSEMOVE:
                if (!CursorCaptured()) {
                    g_cursorX = (float)GET_X_LPARAM(lp);
                    g_cursorY = (float)GET_Y_LPARAM(lp);
                    ClampCursor();
                }
                break;
            case WM_LBUTTONDOWN:
                overlay::OnClick(true);
                break;
            case WM_RBUTTONDOWN:
                overlay::OnClick(false);
                break;
            case WM_KILLFOCUS:
                break;
            default:
                break;
        }
        return CallWindowProcW(g_originalWndProc, hwnd, msg, wp, lp);
    }

    // --------------------------------------------------------- menu is open
    switch (msg) {
        case WM_INPUT: {
            RAWINPUT raw{};
            UINT size = sizeof raw;
            if (CursorCaptured() &&
                GetRawInputData((HRAWINPUT)lp, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
                raw.header.dwType == RIM_TYPEMOUSE &&
                !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                g_cursorX += (float)raw.data.mouse.lLastX;
                g_cursorY += (float)raw.data.mouse.lLastY;
                ClampCursor();
            }
            // Swallow raw input so the camera does not turn behind the menu.
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_MOUSEMOVE:
            if (!CursorCaptured()) {
                g_cursorX = (float)GET_X_LPARAM(lp);
                g_cursorY = (float)GET_Y_LPARAM(lp);
                ClampCursor();
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
            io.AddMouseWheelEvent(0.f, (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA);
            return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursorA(nullptr, IDC_ARROW));
            return TRUE;
        case WM_CHAR:
        case WM_SYSCHAR:
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
            return 0;
        case WM_KILLFOCUS:
            CloseMenu();
            break;
        default:
            break;
    }
    return CallWindowProcW(g_originalWndProc, hwnd, msg, wp, lp);
}

void InitializeForWindow(HWND hwnd) {
    if (!renderer::Init(hwnd)) return;
    overlay::Init();
    g_window = hwnd;
    g_originalWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
    InterlockedExchange(&g_wndProcInstalled, 1);
    Log("hook: attached to hwnd=%p", (void*)hwnd);
}

BOOL WINAPI HookedWglSwapBuffers(HDC dc) {
    if (g_depth == 0) {
        ++g_depth;
        HWND hwnd = WindowFromDC(dc);
        if (!g_window && IsGameWindow(hwnd)) InitializeForWindow(hwnd);
        if (g_window && hwnd == g_window) {
            UpdateCursorVisibility();
            overlay::Render();
        }
        --g_depth;
    }
    return g_originalWgl(dc);
}

BOOL WINAPI HookedGdiSwapBuffers(HDC dc) {
    if (g_depth == 0) {
        ++g_depth;
        HWND hwnd = WindowFromDC(dc);
        if (!g_window && IsGameWindow(hwnd)) InitializeForWindow(hwnd);
        if (g_window && hwnd == g_window) {
            UpdateCursorVisibility();
            overlay::Render();
        }
        --g_depth;
    }
    return g_originalGdi(dc);
}

}  // namespace

bool Installed() { return g_installed != 0; }

HWND Window() { return g_window; }

bool CursorCaptured() {
    if (!g_window) return false;

    CURSORINFO ci{};
    ci.cbSize = sizeof ci;
    if (GetCursorInfo(&ci)) {
        if (ci.hCursor == nullptr || !(ci.flags & CURSOR_SHOWING)) return true;
    }

    RECT clip{}, client{};
    if (GetClipCursor(&clip) && GetClientRect(g_window, &client)) {
        POINT tl{client.left, client.top};
        POINT br{client.right, client.bottom};
        ClientToScreen(g_window, &tl);
        ClientToScreen(g_window, &br);
        const bool matches = std::abs(clip.left - tl.x) <= 2 && std::abs(clip.top - tl.y) <= 2 &&
                             std::abs(clip.right - br.x) <= 2 && std::abs(clip.bottom - br.y) <= 2;
        const bool whole = (clip.right - clip.left) >= GetSystemMetrics(SM_CXVIRTUALSCREEN) &&
                           (clip.bottom - clip.top) >= GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (matches && !whole) return true;
    }
    return false;
}

bool VirtualCursor(ImVec2& clientPixels) {
    clientPixels = ImVec2(g_cursorX, g_cursorY);
    return g_window != nullptr;
}

bool Install() {
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) return true;

    if (MH_Initialize() != MH_OK) {
        Log("hook: MH_Initialize failed");
        InterlockedExchange(&g_installed, 0);
        return false;
    }
    g_minHookReady = true;

    HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
    if (!opengl) opengl = LoadLibraryW(L"opengl32.dll");
    if (opengl) {
        void* target = (void*)GetProcAddress(opengl, "wglSwapBuffers");
        if (target && MH_CreateHook(target, (void*)&HookedWglSwapBuffers, (void**)&g_originalWgl) == MH_OK) {
            MH_EnableHook(target);
            Log("hook: wglSwapBuffers hooked");
        } else {
            Log("hook: wglSwapBuffers hook failed");
        }
    }

    // Fallback for builds that call the GDI export directly (GLFW does on Win32
    // in some configurations). The depth guard prevents a double render.
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    if (gdi) {
        void* target = (void*)GetProcAddress(gdi, "SwapBuffers");
        if (target && MH_CreateHook(target, (void*)&HookedGdiSwapBuffers, (void**)&g_originalGdi) == MH_OK) {
            MH_EnableHook(target);
            Log("hook: SwapBuffers hooked");
        }
    }

    if (!g_originalWgl && !g_originalGdi) {
        Log("hook: no swap function could be hooked");
        MH_Uninitialize();
        g_minHookReady = false;
        InterlockedExchange(&g_installed, 0);
        return false;
    }
    return true;
}

void Uninstall() {
    if (InterlockedCompareExchange(&g_installed, 0, 1) == 0) return;

    if (g_wndProcInstalled && g_window && IsWindow(g_window) && g_originalWndProc) {
        SetWindowLongPtrW(g_window, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
        g_originalWndProc = nullptr;
    }
    InterlockedExchange(&g_wndProcInstalled, 0);

    if (g_minHookReady) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        g_minHookReady = false;
    }

    overlay::Shutdown();
    renderer::Shutdown();
    g_window = nullptr;
    Log("hook: uninstalled");
}

}  // namespace uv::hook
