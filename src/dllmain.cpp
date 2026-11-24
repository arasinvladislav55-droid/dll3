// UniversalVisuals — DLL entry point.
//
// The DLL installs its hooks on a worker thread so DllMain never touches the
// loader lock when MinHook / ImGui / OpenGL initialise.
#include <windows.h>

#include "core/hook.h"
#include "util/log.h"

namespace {

DWORD WINAPI InstallThread(LPVOID) {
    uv::hook::Install();
    return 0;
}

}  // namespace

extern "C" __declspec(dllexport) int JNI_OnLoad(void*, void*) {
    uv::hook::Install();
    return 0x00010008;  // JNI 1.8
}

extern "C" __declspec(dllexport) int Agent_OnLoad(void*, char*, void*) {
    uv::hook::Install();
    return 0;
}

// Optional manual eject: restores the window procedure and removes the hooks.
extern "C" __declspec(dllexport) void UniversalVisuals_Unload() {
    uv::hook::Uninstall();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            uv::Log("dll: attached");
            if (HANDLE thread = CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr))
                CloseHandle(thread);
            break;
        case DLL_PROCESS_DETACH:
            // Only clean up on FreeLibrary; during process termination the game
            // may already have destroyed its GL context.
            if (reserved == nullptr) {
                uv::hook::Uninstall();
                uv::Log("dll: detached");
            }
            break;
        default:
            break;
    }
    return TRUE;
}
