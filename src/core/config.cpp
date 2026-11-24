#include "core/config.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "util/anim.h"
#include "util/log.h"

namespace uv {

Config g_cfg;

namespace {

constexpr unsigned kMagic = 0x56554346;  // "UVCF"

struct FileHeader {
    unsigned magic;
    int version;
    unsigned size;
};

std::wstring ModuleDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&ModuleDirectory), &module))
        return L".";
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return L".";
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return L".";
    *slash = 0;
    return path;
}

std::wstring ConfigPath() { return ModuleDirectory() + L"\\universalvisuals.cfg"; }

}  // namespace

void ResetConfig() {
    g_cfg = Config{};

    // Sensible default layout (top-left column + right side).
    g_cfg.hud[HUD_Watermark] = {true, 0.020f, 0.030f};
    g_cfg.hud[HUD_Fps] = {true, 0.020f, 0.115f};
    g_cfg.hud[HUD_Cps] = {true, 0.020f, 0.205f};
    g_cfg.hud[HUD_Coords] = {true, 0.020f, 0.305f};
    g_cfg.hud[HUD_Armor] = {true, 0.840f, 0.150f};
    g_cfg.hud[HUD_Keystrokes] = {true, 0.800f, 0.700f};
}

void SanitizeConfig() {
    g_cfg.uiScale = Clamp(g_cfg.uiScale, 0.75f, 2.f);
    g_cfg.menuOpacity = Clamp(g_cfg.menuOpacity, 0.55f, 1.f);
    g_cfg.menuAnimSpeed = Clamp(g_cfg.menuAnimSpeed, 4.f, 30.f);
    g_cfg.cursorSensitivity = Clamp(g_cfg.cursorSensitivity, 0.1f, 4.f);
    g_cfg.hudScale = Clamp(g_cfg.hudScale, 0.6f, 1.6f);
    g_cfg.hudSpring = Clamp(g_cfg.hudSpring, 0.4f, 2.5f);
    g_cfg.fogDensity = Clamp(g_cfg.fogDensity, 0.f, 1.f);
    g_cfg.tintStrength = Clamp(g_cfg.tintStrength, 0.f, 0.8f);
    g_cfg.zoomFactor = Clamp(g_cfg.zoomFactor, 0.1f, 0.95f);
    g_cfg.zoomSmooth = Clamp(g_cfg.zoomSmooth, 1.f, 30.f);
    g_cfg.hatScale = Clamp(g_cfg.hatScale, 0.4f, 2.5f);
    g_cfg.hatTilt = Clamp(g_cfg.hatTilt, -60.f, 60.f);
    g_cfg.hatSpin = Clamp(g_cfg.hatSpin, 0.f, 180.f);
    g_cfg.ringLife = Clamp(g_cfg.ringLife, 0.3f, 3.f);
    g_cfg.ringSize = Clamp(g_cfg.ringSize, 0.3f, 3.f);
    g_cfg.trailRate = Clamp(g_cfg.trailRate, 2.f, 120.f);
    g_cfg.trailLife = Clamp(g_cfg.trailLife, 0.3f, 4.f);
    g_cfg.trailSize = Clamp(g_cfg.trailSize, 0.3f, 3.f);
    g_cfg.swingSpeed = Clamp(g_cfg.swingSpeed, 0.3f, 3.f);
    g_cfg.viewScale = Clamp(g_cfg.viewScale, 0.5f, 2.f);
    g_cfg.timeValue = (int)Clamp((float)g_cfg.timeValue, 0.f, 24000.f);

    for (int i = 0; i < kHudElementCount; ++i) {
        g_cfg.hud[i].x = Clamp(g_cfg.hud[i].x, 0.f, 0.98f);
        g_cfg.hud[i].y = Clamp(g_cfg.hud[i].y, 0.f, 0.96f);
    }

    // A zero keybind means "unbound", which is allowed, but avoid VK 0xFF etc.
    const int keys[] = {g_cfg.keyMenu, g_cfg.keyToggle, g_cfg.keyZoom};
    (void)keys;
}

bool SaveConfig() {
    const std::wstring path = ConfigPath();
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || !file) {
        Log("config: cannot open '%ls' for writing", path.c_str());
        return false;
    }
    FileHeader header{kMagic, kConfigVersion, (unsigned)sizeof(Config)};
    const bool ok = std::fwrite(&header, sizeof header, 1, file) == 1 &&
                    std::fwrite(&g_cfg, sizeof g_cfg, 1, file) == 1;
    std::fclose(file);
    if (ok) Log("config: saved (%u bytes)", (unsigned)sizeof(Config));
    return ok;
}

bool LoadConfig() {
    const std::wstring path = ConfigPath();
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) {
        Log("config: no file, using defaults");
        ResetConfig();
        return false;
    }
    FileHeader header{};
    bool ok = std::fread(&header, sizeof header, 1, file) == 1;
    ok = ok && header.magic == kMagic && header.version == kConfigVersion &&
         header.size == sizeof(Config);
    if (ok) {
        Config loaded{};
        ok = std::fread(&loaded, sizeof loaded, 1, file) == 1;
        if (ok) g_cfg = loaded;
    }
    std::fclose(file);

    if (!ok) {
        Log("config: incompatible or corrupt file, resetting");
        ResetConfig();
        SaveConfig();
        return false;
    }
    SanitizeConfig();
    Log("config: loaded");
    return true;
}

const wchar_t* ConfigPathW() {
    static std::wstring path = ConfigPath();
    return path.c_str();
}

const char* ConfigPathA() {
    static std::string narrow;
    const std::wstring& wide = ConfigPathW();
    int size = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        narrow.resize((size_t)size);
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, narrow.data(), size, nullptr, nullptr);
        narrow.pop_back();
    }
    return narrow.c_str();
}

}  // namespace uv
