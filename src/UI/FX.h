#pragma once
#include "imgui.h"

// Windows headers BẮT BUỘC trước mmsystem.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include <cmath>
#include <cstring>
#include <cstdlib>

namespace FX {

// ---- Easing ----
inline float EaseOutCubic(float t){ return 1.f - powf(1.f-t, 3.f); }
inline float EaseOutBack(float t){
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    return 1.f + c3*powf(t-1,3) + c1*powf(t-1,2);
}
inline float EaseOutElastic(float t){
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    const float c4 = (2.f*3.14159f)/3.f;
    return powf(2.f,-10.f*t)*sinf((t*10.f - 0.75f)*c4) + 1.f;
}

// ---- Data ----
struct Spark { float x,y,vx,vy,life; ImU32 color; };

struct Confetti {
    float x,y,vx,vy,rot,vrot,life;
    ImU32 color;
    float w,h;
};

struct NumberPop {
    float x,y,vy,life;
    char text[32];
    ImU32 color;
};

struct TrailPt { float x,y,life; };

struct WidgetFX {
    float tickAnim    = 0.f;
    float squashT     = -1.f;
    float squashScale = 1.f;
    float shakeT      = -1.f;
    float flashT      = -1.f;
    float ringT       = -1.f;
    ImVec2 ringPos    = ImVec2(0,0);
    float rippleT     = -1.f;
    float trailX      = -1.f;
    float trailT      = -1.f;
    Spark sparks[8]   = {};
    int   sparkCount  = 0;
    float morphT      = -1.f;
    float glowAmt     = 0.f;

    static const int MAX_CONFETTI = 20;
    Confetti confetti[MAX_CONFETTI] = {};
    int      confettiCount = 0;

    static const int MAX_TRAIL = 24;
    TrailPt  trailPts[MAX_TRAIL] = {};
    int      trailHead = 0;
    float    lastTrailSpawn = 0.f;

    static const int MAX_POPS = 4;
    NumberPop pops[MAX_POPS] = {};
    int       popCount = 0;
    float     lastPopTime = 0.f;
};

static const int MAX_FX = 128;
static WidgetFX g_fx[MAX_FX];
static float    g_now = 0.f;

static float g_screenShakeT   = -1.f;
static float g_screenShakeAmp = 0.f;
static float g_cursorPulseT   = -1.f;

inline WidgetFX& Get(int id){ return g_fx[id & (MAX_FX-1)]; }
inline void BeginFrame(float dt){ g_now += dt; }

// ---- Sound ----
// Khai báo MessageBeep — nằm trong user32.lib (đã link sẵn)
extern "C" __declspec(dllimport) int __stdcall MessageBeep(unsigned int uType);

inline void PlayFXSound(SoundType type) {
    switch (type) {
        case SND_CLICK:   MessageBeep(0x00000000); break;  // MB_OK
        case SND_CHECK:   MessageBeep(0x00000040); break;  // MB_ICONASTERISK
        case SND_UNCHECK: MessageBeep(0x00000010); break;  // MB_ICONHAND
        case SND_SLIDE:   MessageBeep(0x00000020); break;  // MB_ICONQUESTION
        case SND_SUCCESS: MessageBeep(0x00000030); break;  // MB_ICONEXCLAMATION
    }
}

// ---- Screen shake ----
inline void TriggerScreenShake(float amp = 4.f) {
    g_screenShakeT = g_now;
    g_screenShakeAmp = amp;
}

inline ImVec2 GetScreenShake() {
    if (g_screenShakeT < 0.f) return ImVec2(0,0);
    float t = g_now - g_screenShakeT;
    if (t > 0.35f) { g_screenShakeT = -1.f; return ImVec2(0,0); }
    float decay = 1.f - t/0.35f;
    float a = g_screenShakeAmp * decay;
    return ImVec2(sinf(t*80.f)*a, cosf(t*95.f)*a*0.6f);
}

// ---- Cursor pulse ----
inline void TriggerCursorPulse() { g_cursorPulseT = g_now; }

inline void DrawCursorPulse(ImDrawList* dl) {
    if (g_cursorPulseT < 0.f) return;
    float t = g_now - g_cursorPulseT;
    if (t > 0.3f) { g_cursorPulseT = -1.f; return; }
    ImVec2 m = ImGui::GetIO().MousePos;
    float p = t / 0.3f;
    float r = 8.f + 20.f * EaseOutCubic(p);
    int a = (int)(140 * (1.f - p));
    dl->AddCircle(m, r, IM_COL32(80,190,120,a), 0, 2.f);
    dl->AddCircle(m, r*0.7f, IM_COL32(80,190,120,a/2), 0, 1.f);
}

// ---- OnCheckChanged ----
inline void OnCheckChanged(int id, bool newState, ImVec2 center) {
    auto& f = Get(id);
    f.tickAnim = 0.f;
    f.flashT   = g_now;
    f.ringT    = g_now;
    f.ringPos  = center;
    f.shakeT   = g_now;
    f.rippleT  = g_now;
    f.morphT   = g_now;
    f.squashT  = g_now;
    f.glowAmt  = 1.f;

    if (newState) {
        f.sparkCount = 8;
        for (int i = 0; i < 8; ++i) {
            float a = (float)i/8.f * 6.2831853f;
            f.sparks[i].x = center.x;
            f.sparks[i].y = center.y;
            f.sparks[i].vx = cosf(a)*50.f;
            f.sparks[i].vy = sinf(a)*50.f;
            f.sparks[i].life = 1.f;
            f.sparks[i].color = IM_COL32(255,220,100,255);
        }
    } else {
        f.sparkCount = 0;
    }

    if (newState) {
        f.confettiCount = WidgetFX::MAX_CONFETTI;
        for (int i = 0; i < WidgetFX::MAX_CONFETTI; ++i) {
            auto& c = f.confetti[i];
            float ang = ((float)rand()/RAND_MAX) * 6.2831853f;
            float spd = 60.f + ((float)rand()/RAND_MAX) * 100.f;
            c.x = center.x;
            c.y = center.y;
            c.vx = cosf(ang) * spd;
            c.vy = sinf(ang) * spd - 80.f;
            c.rot = ((float)rand()/RAND_MAX) * 6.28f;
            c.vrot = ((float)rand()/RAND_MAX - 0.5f) * 15.f;
            c.life = 1.f;
            c.w = 4.f + ((float)rand()/RAND_MAX) * 4.f;
            c.h = 3.f + ((float)rand()/RAND_MAX) * 3.f;
            static const ImU32 cols[] = {
                IM_COL32(255,100,100,255),
                IM_COL32(100,200,255,255),
                IM_COL32(255,220,100,255),
                IM_COL32(120,255,140,255),
                IM_COL32(255,150,220,255),
                IM_COL32(180,150,255,255),
            };
            c.color = cols[rand() % 6];
        }
    }

    PlayFXSound(newState ? SND_CHECK : SND_UNCHECK);
    TriggerScreenShake(newState ? 3.5f : 2.f);
    TriggerCursorPulse();
}

// ---- Slider popups ----
inline void OnSliderChanged(int id, float value, ImVec2 pos, const char* fmt = "%.0f") {
    auto& f = Get(id);
    if (g_now - f.lastPopTime < 0.12f) return;
    f.lastPopTime = g_now;

    int slot = -1;
    for (int i = 0; i < WidgetFX::MAX_POPS; ++i)
        if (f.pops[i].life <= 0.f) { slot = i; break; }
    if (slot < 0) slot = f.popCount % WidgetFX::MAX_POPS;
    f.popCount++;

    auto& p = f.pops[slot];
    p.x = pos.x;
    p.y = pos.y;
    p.vy = -30.f;
    p.life = 1.f;
    p.color = IM_COL32(80,190,120,255);
    snprintf(p.text, sizeof(p.text), fmt, value);
}

// ---- Update ----
inline void TickUpdate(int id, bool checked, float dt) {
    auto& f = Get(id);
    float target = checked ? 1.f : 0.f;
    f.tickAnim += (target - f.tickAnim) * (1.f - expf(-12.f*dt));

    if (f.squashT > 0.f) {
        float t = g_now - f.squashT;
        if (t < 0.4f) {
            float decay = expf(-12.f*t);
            float osc   = cosf(sqrtf(200.f)*t);
            f.squashScale = 1.f + 0.25f * decay * (-osc);
        } else f.squashScale = 1.f;
    } else f.squashScale = 1.f;

    f.glowAmt *= expf(-4.f*dt);
}

inline void UpdateParticles(int id, float dt) {
    auto& f = Get(id);

    for (int i = 0; i < f.sparkCount; ++i) {
        auto& s = f.sparks[i];
        if (s.life <= 0.f) continue;
        s.x += s.vx*dt; s.y += s.vy*dt;
        s.vx *= 0.92f; s.vy *= 0.92f;
        s.vy += 100.f*dt;
        s.life -= dt*1.5f;
        if (s.life < 0.f) s.life = 0.f;
    }

    if (f.confettiCount > 0) {
        bool anyAlive = false;
        for (int i = 0; i < f.confettiCount; ++i) {
            auto& c = f.confetti[i];
            if (c.life <= 0.f) continue;
            anyAlive = true;
            c.x += c.vx*dt; c.y += c.vy*dt;
            c.vy += 180.f*dt;
            c.vx *= 0.98f;
            c.rot += c.vrot*dt;
            c.life -= dt*0.7f;
            if (c.life < 0.f) c.life = 0.f;
        }
        if (!anyAlive) f.confettiCount = 0;
    }

    for (int i = 0; i < WidgetFX::MAX_TRAIL; ++i) {
        auto& tp = f.trailPts[i];
        if (tp.life <= 0.f) continue;
        tp.y -= 15.f*dt;
        tp.life -= dt*2.5f;
        if (tp.life < 0.f) tp.life = 0.f;
    }

    for (int i = 0; i < WidgetFX::MAX_POPS; ++i) {
        auto& p = f.pops[i];
        if (p.life <= 0.f) continue;
        p.y += p.vy*dt;
        p.vy *= 0.94f;
        p.life -= dt*1.2f;
        if (p.life < 0.f) p.life = 0.f;
    }

    if (f.trailT > 0.f && g_now - f.trailT > 0.3f) f.trailT = -1.f;
}

// ---- Draw box FX ----
inline void DrawFX(ImDrawList* dl, int id, ImVec2 boxMin, ImVec2 boxMax) {
    auto& f = Get(id);
    float boxW = boxMax.x - boxMin.x;

    if (f.ringT > 0.f) {
        float t = g_now - f.ringT;
        if (t < 0.6f) {
            float p = t/0.6f;
            float r = 8.f + 24.f*EaseOutCubic(p);
            int a = (int)(120*(1.f-p));
            dl->AddCircle(f.ringPos, r, IM_COL32(80,80,80,a), 0, 2.f);
        }
    }
    if (f.flashT > 0.f) {
        float t = g_now - f.flashT;
        if (t < 0.35f) {
            float p = t/0.35f;
            int a = (int)(90*(1.f-p));
            dl->AddRectFilled(boxMin, boxMax, IM_COL32(80,190,120,a), 3.f);
        }
    }
    if (f.rippleT > 0.f) {
        float t = g_now - f.rippleT;
        if (t < 0.5f) {
            float p = t/0.5f;
            float x = boxMin.x + boxW*EaseOutCubic(p);
            int a = (int)(80*(1.f-p));
            dl->AddRectFilled(ImVec2(x-6, boxMin.y),
                              ImVec2(x+6, boxMax.y),
                              IM_COL32(255,255,255,a), 3.f);
        }
    }
    for (int i = 0; i < f.sparkCount; ++i) {
        auto& s = f.sparks[i];
        if (s.life <= 0.f) continue;
        ImU32 col = (s.color & 0x00FFFFFF) | ((int)(255*s.life) << 24);
        dl->AddCircleFilled(ImVec2(s.x, s.y), 2.f*s.life, col);
    }
    if (f.glowAmt > 0.01f) {
        int a = (int)(80*f.glowAmt);
        dl->AddRect(ImVec2(boxMin.x-2, boxMin.y-2),
                    ImVec2(boxMax.x+2, boxMax.y+2),
                    IM_COL32(80,190,120,a), 5.f, 0, 2.f);
    }
    if (f.trailT > 0.f) {
        float t = g_now - f.trailT;
        if (t < 0.3f) {
            float p = 1.f - t/0.3f;
            float w = boxW * 0.6f * p;
            dl->AddRectFilled(ImVec2(f.trailX - w*0.5f, boxMin.y),
                              ImVec2(f.trailX + w*0.5f, boxMax.y),
                              IM_COL32(255,255,255,(int)(60*p)), 3.f);
        }
    }
}

// ---- Draw particles ----
inline void DrawParticles(ImDrawList* dl, int id) {
    auto& f = Get(id);

    for (int i = 0; i < f.confettiCount; ++i) {
        auto& c = f.confetti[i];
        if (c.life <= 0.f) continue;
        ImU32 col = (c.color & 0x00FFFFFF) | ((int)(255*c.life) << 24);

        float cosr = cosf(c.rot), sinr = sinf(c.rot);
        float hw = c.w*0.5f, hh = c.h*0.5f;
        ImVec2 pts[4] = {
            ImVec2(c.x + (-hw*cosr - -hh*sinr), c.y + (-hw*sinr + -hh*cosr)),
            ImVec2(c.x + ( hw*cosr - -hh*sinr), c.y + ( hw*sinr + -hh*cosr)),
            ImVec2(c.x + ( hw*cosr -  hh*sinr), c.y + ( hw*sinr +  hh*cosr)),
            ImVec2(c.x + (-hw*cosr -  hh*sinr), c.y + (-hw*sinr +  hh*cosr)),
        };
        dl->AddConvexPolyFilled(pts, 4, col);
    }

    for (int i = 0; i < WidgetFX::MAX_POPS; ++i) {
        auto& p = f.pops[i];
        if (p.life <= 0.f) continue;
        int a = (int)(255 * p.life);
        ImU32 col = (p.color & 0x00FFFFFF) | (a << 24);
        dl->AddText(ImVec2(p.x, p.y), col, p.text);
    }
}

inline float GetShake(int id) {
    auto& f = Get(id);
    if (f.shakeT < 0.f) return 0.f;
    float t = g_now - f.shakeT;
    if (t > 0.3f) return 0.f;
    return sinf(t*60.f) * 3.f * (1.f - t/0.3f);
}

inline float GetSquash(int id) { return Get(id).squashScale; }

} // namespace FX
