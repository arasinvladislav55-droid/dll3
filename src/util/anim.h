// UniversalVisuals — math & animation helpers.
#pragma once

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace uv {

inline float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float LerpUnclamped(float a, float b, float t) { return a + (b - a) * t; }

inline ImVec2 Lerp(const ImVec2& a, const ImVec2& b, float t) {
    return ImVec2(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t));
}
inline ImVec2 Add(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
inline ImVec2 Sub(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
inline ImVec2 Mul(const ImVec2& a, float s) { return ImVec2(a.x * s, a.y * s); }
inline float Length(const ImVec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline bool InRect(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
    return p.x >= a.x && p.y >= a.y && p.x <= b.x && p.y <= b.y;
}

// Frame-rate independent exponential smoothing.
inline float SmoothTowards(float current, float target, float speed, float dt) {
    if (speed <= 0.f || dt <= 0.f) return target;
    const float k = 1.f - std::exp(-speed * dt);
    return current + (target - current) * k;
}
inline ImVec2 SmoothTowards(const ImVec2& current, const ImVec2& target, float speed, float dt) {
    return ImVec2(SmoothTowards(current.x, target.x, speed, dt),
                  SmoothTowards(current.y, target.y, speed, dt));
}

inline float EaseOutCubic(float t) {
    t = Clamp01(t);
    const float u = 1.f - t;
    return 1.f - u * u * u;
}
inline float EaseOutQuint(float t) {
    t = Clamp01(t);
    const float u = 1.f - t;
    return 1.f - u * u * u * u * u;
}
inline float EaseInOutCubic(float t) {
    t = Clamp01(t);
    return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) * 0.5f;
}
inline float EaseOutBack(float t) {
    t = Clamp01(t);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.f;
    const float u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}

// Critically-ish damped spring for UI motion.
struct Spring {
    float value = 0.f;
    float velocity = 0.f;

    void Reset(float v) {
        value = v;
        velocity = 0.f;
    }

    float Update(float target, float dt, float stiffness = 260.f, float damping = 24.f) {
        if (dt <= 0.f) return value;
        dt = std::min(dt, 1.f / 30.f);
        const float force = stiffness * (target - value) - damping * velocity;
        velocity += force * dt;
        value += velocity * dt;
        if (std::fabs(target - value) < 0.0004f && std::fabs(velocity) < 0.0004f) {
            value = target;
            velocity = 0.f;
        }
        return value;
    }
};

// Simple smoothed scalar (non-overshooting).
struct Anim {
    float value = 0.f;
    float Update(float target, float speed, float dt) {
        value = SmoothTowards(value, target, speed, dt);
        return value;
    }
};

// One-shot pulse used for hover/press glows.
struct Pulse {
    float value = 0.f;
    void Trigger() { value = 1.f; }
    float Update(float dt, float decay = 4.f) {
        value = std::max(0.f, value - decay * dt);
        return value;
    }
};

inline ImU32 Col(float r, float g, float b, float a = 1.f) {
    return IM_COL32((int)(Clamp01(r) * 255.f + 0.5f), (int)(Clamp01(g) * 255.f + 0.5f),
                    (int)(Clamp01(b) * 255.f + 0.5f), (int)(Clamp01(a) * 255.f + 0.5f));
}

inline ImU32 Alpha(ImU32 c, float mul) {
    const int a = (int)((float)((c >> IM_COL32_A_SHIFT) & 0xFF) * Clamp01(mul) + 0.5f);
    return (c & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
}

inline ImU32 Mix(ImU32 a, ImU32 b, float t) {
    t = Clamp01(t);
    const int ar = (a >> IM_COL32_R_SHIFT) & 0xFF, ag = (a >> IM_COL32_G_SHIFT) & 0xFF,
              ab = (a >> IM_COL32_B_SHIFT) & 0xFF, aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
    const int br = (b >> IM_COL32_R_SHIFT) & 0xFF, bg = (b >> IM_COL32_G_SHIFT) & 0xFF,
              bb = (b >> IM_COL32_B_SHIFT) & 0xFF, ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32((int)Lerp((float)ar, (float)br, t), (int)Lerp((float)ag, (float)bg, t),
                    (int)Lerp((float)ab, (float)bb, t), (int)Lerp((float)aa, (float)ba, t));
}

// HSV helpers (h in [0,1)).
inline ImVec4 Hsv(float h, float s, float v, float a = 1.f) {
    h = h - std::floor(h);
    const float i = std::floor(h * 6.f);
    const float f = h * 6.f - i;
    const float p = v * (1.f - s);
    const float q = v * (1.f - f * s);
    const float t = v * (1.f - (1.f - f) * s);
    switch ((int)i % 6) {
        case 0: return ImVec4(v, t, p, a);
        case 1: return ImVec4(q, v, p, a);
        case 2: return ImVec4(p, v, t, a);
        case 3: return ImVec4(p, q, v, a);
        case 4: return ImVec4(t, p, v, a);
        default: return ImVec4(v, p, q, a);
    }
}
inline float HueOf(const ImVec4& c) {
    const float mx = std::max(c.x, std::max(c.y, c.z));
    const float mn = std::min(c.x, std::min(c.y, c.z));
    const float d = mx - mn;
    if (d < 1e-5f) return 0.f;
    float h;
    if (mx == c.x) h = (c.y - c.z) / d + (c.y < c.z ? 6.f : 0.f);
    else if (mx == c.y) h = (c.z - c.x) / d + 2.f;
    else h = (c.x - c.y) / d + 4.f;
    return h / 6.f;
}

}  // namespace uv
