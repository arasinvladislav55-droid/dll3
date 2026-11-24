#include "gfx/visuals.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/bridge.h"
#include "core/config.h"
#include "gfx/renderer.h"
#include "gfx/theme.h"
#include "util/anim.h"

namespace uv::visuals {
namespace {

struct Ring {
    ImVec2 pos;
    float age = 0.f;
    float life = 1.f;
    float radius = 20.f;
    float aspect = 0.34f;
};

struct Particle {
    ImVec2 pos;
    ImVec2 vel;
    float age = 0.f;
    float life = 1.f;
    float radius = 2.f;
    float seed = 0.f;
};

std::vector<Ring> g_rings;
std::vector<Particle> g_particles;

float g_time = 0.f;
float g_trailAccum = 0.f;
float g_demoRingTimer = 0.f;
float g_zoom = 0.f;

bool g_hadPlayer = false;
bool g_prevOnGround = true;

int g_lastTime = -1;
bool g_lastTimeOn = false;

void EllipsePoints(ImVector<ImVec2>& out, ImVec2 center, float rx, float ry, int segments) {
    out.clear();
    out.reserve(segments);
    for (int i = 0; i < segments; ++i) {
        const float a = (float)i / (float)segments * 6.2831853f;
        out.push_back(ImVec2(center.x + std::cos(a) * rx, center.y + std::sin(a) * ry));
    }
}

void Ellipse(ImDrawList* dl, ImVec2 center, float rx, float ry, ImU32 color, float thickness, int segments = 56) {
    ImVector<ImVec2> points;
    EllipsePoints(points, center, rx, ry, segments);
    dl->AddPolyline(points.Data, points.Size, color, ImDrawFlags_Closed, thickness);
}

void EllipseFilled(ImDrawList* dl, ImVec2 center, float rx, float ry, ImU32 color, int segments = 56) {
    ImVector<ImVec2> points;
    EllipsePoints(points, center, rx, ry, segments);
    dl->AddConvexPolyFilled(points.Data, points.Size, color);
}

void DrawChinaHat(const bridge::PlayerState& player) {
    if (!cfg.hatOn || !player.head.valid) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const float base = std::max(8.f, player.head.radius * 1.65f * cfg.hatScale);
    const float bob = std::sin(g_time * 1.7f) * base * 0.05f;
    const ImVec2 center(player.head.screen.x, player.head.screen.y + bob);
    const ImVec2 apex(center.x + std::sin(cfg.hatTilt * 0.01745f) * base * 0.42f, center.y - base * 1.20f);
    const float rx = base;
    const float ry = base * 0.30f;
    const float spin = g_time * cfg.hatSpin * 0.01745f;
    const int segments = 40;

    const ImU32 colorA = Col(cfg.hatColorA.r, cfg.hatColorA.g, cfg.hatColorA.b, 1.f);
    const ImU32 colorB = Col(cfg.hatColorB.r, cfg.hatColorB.g, cfg.hatColorB.b, 1.f);

    // Soft glow under the hat.
    renderer::SoftCircle(dl, ImVec2(center.x, center.y + 2.f), base * 0.9f, Alpha(colorA, 0.10f), 5);

    // Cone body: per-segment quads with a moving highlight.
    for (int i = 0; i < segments; ++i) {
        const float a0 = (float)i / (float)segments * 6.2831853f + spin;
        const float a1 = (float)(i + 1) / (float)segments * 6.2831853f + spin;
        const ImVec2 p0(center.x + std::cos(a0) * rx, center.y + std::sin(a0) * ry);
        const ImVec2 p1(center.x + std::cos(a1) * rx, center.y + std::sin(a1) * ry);
        const float light = 0.45f + 0.55f * (std::sin(a0 + spin * 0.5f) * 0.5f + 0.5f);
        const ImU32 body = Alpha(Mix(colorA, colorB, light), 0.30f + 0.45f * light);
        dl->AddQuadFilled(apex, p0, p1, apex, body);
    }

    // Rim.
    EllipseFilled(dl, center, rx, ry, Alpha(colorA, 0.16f));
    Ellipse(dl, center, rx, ry, Alpha(Mix(colorA, colorB, 0.6f), 0.95f), 2.2f);
    Ellipse(dl, center, rx * 0.82f, ry * 0.82f, Alpha(colorB, 0.35f), 1.f);

    // Highlight slope + apex sparkle.
    dl->AddLine(ImVec2(apex.x, apex.y + 2.f), ImVec2(center.x + rx * 0.35f, center.y), Alpha(colorB, 0.55f), 1.2f);
    const float pulse = 0.55f + 0.45f * std::sin(g_time * 3.1f);
    renderer::SoftCircle(dl, ImVec2(apex.x, apex.y + 1.f), base * 0.16f, Alpha(colorB, 0.55f * pulse), 4);
    dl->AddCircleFilled(ImVec2(apex.x, apex.y), base * 0.055f, IM_COL32(255, 255, 255, 240), 12);
}

void SpawnRing(ImVec2 pos, float radius) {
    Ring ring;
    ring.pos = pos;
    ring.age = 0.f;
    ring.life = std::max(0.15f, cfg.ringLife);
    ring.radius = radius * std::max(0.2f, cfg.ringSize);
    g_rings.push_back(ring);
    if (g_rings.size() > 20) g_rings.erase(g_rings.begin());
}

void UpdateAndDrawRings(float dt) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 color = Col(cfg.ringColor.r, cfg.ringColor.g, cfg.ringColor.b, 1.f);

    for (size_t i = 0; i < g_rings.size();) {
        Ring& ring = g_rings[i];
        ring.age += dt;
        if (ring.age >= ring.life) {
            g_rings[i] = g_rings.back();
            g_rings.pop_back();
            continue;
        }
        const float t = ring.age / ring.life;
        const float fade = std::pow(1.f - t, 1.65f);
        const float radius = ring.radius * Lerp(0.55f, 2.7f, EaseOutCubic(t));
        const float ry = radius * ring.aspect;

        Ellipse(dl, ring.pos, radius, ry, Alpha(color, 0.75f * fade), 2.2f);
        Ellipse(dl, ring.pos, radius * 0.90f, ry * 0.90f, Alpha(color, 0.30f * fade), 4.5f);
        if (t < 0.35f) {
            const float glow = (1.f - t / 0.35f);
            EllipseFilled(dl, ring.pos, radius, ry, Alpha(color, 0.10f * glow));
        }
        ++i;
    }
}

void SpawnTrailParticle(ImVec2 origin, float radius) {
    Particle particle;
    const float angle = (float)(rand() % 628) / 100.f;
    const float distance = (float)(rand() % 100) / 100.f * radius;
    const float r = radius * (0.55f + 0.45f * (float)(rand() % 100) / 100.f);
    particle.pos = ImVec2(origin.x + std::cos(angle) * distance, origin.y + std::sin(angle) * distance * 0.4f);
    particle.vel = ImVec2(std::cos(angle) * 4.f, -14.f - (float)(rand() % 100) / 100.f * 14.f);
    particle.life = std::max(0.2f, cfg.trailLife) * (0.6f + 0.6f * (float)(rand() % 100) / 100.f);
    particle.radius = std::max(1.f, cfg.trailSize * (1.3f + (float)(rand() % 100) / 100.f * 1.6f)) * r * 0.045f;
    particle.seed = (float)(rand() % 628) / 100.f;
    g_particles.push_back(particle);
    if (g_particles.size() > 220) g_particles.erase(g_particles.begin());
}

void UpdateAndDrawTrail(float dt) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 color = Col(cfg.trailColor.r, cfg.trailColor.g, cfg.trailColor.b, 1.f);

    for (size_t i = 0; i < g_particles.size();) {
        Particle& p = g_particles[i];
        p.age += dt;
        if (p.age >= p.life) {
            g_particles[i] = g_particles.back();
            g_particles.pop_back();
            continue;
        }
        const float t = p.age / p.life;
        p.pos.x += (p.vel.x + std::sin(g_time * 2.5f + p.seed) * 8.f) * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.y += 9.f * dt;

        const float fade = std::pow(1.f - t, 1.5f);
        const float radius = p.radius * Lerp(1.f, 0.35f, t);
        renderer::SoftCircle(dl, p.pos, radius, Alpha(color, 0.55f * fade), 3);
        dl->AddCircleFilled(p.pos, std::max(0.6f, radius * 0.45f), IM_COL32(255, 255, 255, (int)(220 * fade)), 10);
        ++i;
    }
}

void ApplyWorldSettings() {
    if (cfg.timeOn && (!g_lastTimeOn || g_lastTime != cfg.timeValue)) {
        bridge::SetWorldTime(cfg.timeValue);
        g_lastTime = cfg.timeValue;
        g_lastTimeOn = true;
    }
    if (!cfg.timeOn) g_lastTimeOn = false;

    static float fogLast[4] = {-1.f, -1.f, -1.f, -1.f};
    static bool fogWasOn = false;
    const float fogNow[4] = {cfg.fogColor.r, cfg.fogColor.g, cfg.fogColor.b, cfg.fogDensity};
    if (cfg.fogOn && (!fogWasOn || fogNow[0] != fogLast[0] || fogNow[1] != fogLast[1] ||
                      fogNow[2] != fogLast[2] || fogNow[3] != fogLast[3])) {
        bridge::SetFog(fogNow[0], fogNow[1], fogNow[2], fogNow[3]);
        for (int i = 0; i < 4; ++i) fogLast[i] = fogNow[i];
        fogWasOn = true;
    }
    if (!cfg.fogOn) fogWasOn = false;

    // Smooth zoom (OptiFine style). The base FOV is a placeholder until the game
    // bridge reports the real value; bridge::SetFov is a no-op until wired.
    const bool holding = cfg.zoomOn && (GetAsyncKeyState(cfg.keyZoom) & 0x8000) != 0;
    g_zoom = SmoothTowards(g_zoom, holding ? 1.f : 0.f, cfg.zoomSmooth, ImGui::GetIO().DeltaTime);
    if (cfg.zoomOn || g_zoom > 0.0005f) {
        bridge::SetFov(70.f * Lerp(1.f, cfg.zoomFactor, EaseOutCubic(g_zoom)));
    }

    if (cfg.viewmodelOn) {
        bridge::ApplyViewModel(cfg.viewX, cfg.viewY, cfg.viewZ, cfg.viewScale,
                               cfg.smoothSwing ? cfg.swingSpeed : 1.f);
    }
}

}  // namespace

void DrawWorld(bool showWorld) {
    const float dt = ImGui::GetIO().DeltaTime;
    g_time += dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 display = renderer::DisplaySize();

    if (showWorld && cfg.tintOn) {
        const ImU32 tint = Col(cfg.tintColor.r, cfg.tintColor.g, cfg.tintColor.b, cfg.tintStrength * 0.55f);
        dl->AddRectFilledMultiColor(ImVec2(0, 0), display, tint, Alpha(tint, 0.35f), Alpha(tint, 0.55f), tint);
        // Cinematic vignette.
        const int layers = 5;
        for (int i = 0; i < layers; ++i) {
            const float t = (float)i / (float)layers;
            const float inset = display.y * (0.04f + t * 0.30f);
            const int a = (int)((1.f - t) * 26.f * cfg.tintStrength * 100.f);
            dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, inset), IM_COL32(0, 0, 0, a),
                                        IM_COL32(0, 0, 0, a), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
            dl->AddRectFilledMultiColor(ImVec2(0, display.y - inset), ImVec2(display.x, display.y),
                                        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, a),
                                        IM_COL32(0, 0, 0, a));
        }
    }

    if (showWorld) ApplyWorldSettings();
}

void DrawCosmetics(bool showWorld) {
    if (!showWorld) {
        g_rings.clear();
        g_particles.clear();
        g_hadPlayer = false;
        return;
    }

    const float dt = ImGui::GetIO().DeltaTime;
    const bridge::PlayerState& player = bridge::GetPlayer();
    const bool demo = bridge::Demo();

    if (cfg.ringsOn) {
        if (player.valid) {
            if (g_hadPlayer && player.feet.valid && g_prevOnGround && !player.onGround) {
                SpawnRing(ImVec2(player.feet.screen.x, player.feet.screen.y + player.feet.radius * 0.18f),
                          player.feet.radius * 1.35f);
            }
            if (demo) {
                g_demoRingTimer += dt;
                if (g_demoRingTimer > 2.1f) {
                    g_demoRingTimer = 0.f;
                    if (player.feet.valid)
                        SpawnRing(ImVec2(player.feet.screen.x, player.feet.screen.y + player.feet.radius * 0.18f),
                                  player.feet.radius * 1.35f);
                }
            }
        }
        g_prevOnGround = player.onGround;
        g_hadPlayer = player.valid;
    }

    if (cfg.trailOn && player.valid && player.feet.valid) {
        const bool moving = player.speed > 0.6f || !player.onGround;
        g_trailAccum += (moving ? cfg.trailRate : 0.f) * dt;
        while (g_trailAccum >= 1.f) {
            g_trailAccum -= 1.f;
            SpawnTrailParticle(ImVec2(player.feet.screen.x, player.feet.screen.y), player.feet.radius);
        }
    }

    if (cfg.ringsOn) UpdateAndDrawRings(dt);
    if (cfg.trailOn) UpdateAndDrawTrail(dt);
    DrawChinaHat(player);
}

}  // namespace uv::visuals
