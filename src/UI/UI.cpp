#include "UI.h"
#include "FX.h"
#include "../Launcher.h"
#include "../Settings.h"
#include "../Minecraft.h"
#include "../ModManager.h"
#include "../DiscordRPC.h"
#include "../AccountManager.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "embedded_assets.h"

#include <d3d11.h>
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>

#include <string>
#include <cstdio>
#include <cmath>
#include <cstring>

extern ID3D11Device*        g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern ImFont*              g_fontRegular;
extern ImFont*              g_fontBold;
extern ImFont*              g_fontBig;
extern void LoadTextureFromMemory(const unsigned char*, size_t,
                                  ID3D11ShaderResourceView**);

ID3D11ShaderResourceView*        g_logoTex   = nullptr;
static ID3D11ShaderResourceView* g_bgTex     = nullptr;
static ID3D11ShaderResourceView* g_bannerTex = nullptr;
static bool g_logoTried = false, g_bgTried = false, g_bannerTried = false;

// ---- Kaminari Light Palette ----
static inline ImU32 U32(int r,int g,int b,int a=255){ return IM_COL32(r,g,b,a); }

static const ImU32 K_BG        = U32(238,236,230);
static const ImU32 K_SIDEBAR   = U32(232,230,222);
static const ImU32 K_CARD      = U32(255,255,255);
static const ImU32 K_CARD_ALT  = U32(248,246,240);
static const ImU32 K_HOVER     = U32(225,222,214);
static const ImU32 K_STROKE    = U32(0,0,0,14);
static const ImU32 K_STROKE_HI = U32(0,0,0,25);
static const ImU32 K_TEXT      = U32( 30, 28, 24);
static const ImU32 K_TEXT_DIM  = U32(120,116,108);
static const ImU32 K_TEXT_DIM2 = U32(160,156,148);
static const ImU32 K_ACTIVE    = U32( 20, 20, 20);
static const ImU32 K_GREEN     = U32( 80,190,120);
static const ImU32 K_RED       = U32(220, 80, 80);

static char  g_newAccBuf[64] = "";
static float g_dt      = 1.f/60.f;
static float g_timeNow = 0.f;

static inline float ExpSmooth(float cur, float tgt, float sp) {
    float k = 1.f - expf(-sp * g_dt);
    return cur + (tgt - cur) * k;
}
static inline float EaseOutCubic(float t) { return 1.f - powf(1.f-t, 3.f); }

static int   g_tabSub        = 0;
static float g_tabSubX       = -1.f;
static float g_tabSubHover[6]= {0};

static bool  g_check_enabled   = true;
static bool  g_check_nonAuto   = true;
static bool  g_check_silent    = true;
static bool  g_check_noSpread  = false;
static bool  g_check_extrap    = false;

static float g_fov        = 180.f;
static float g_hitChance  = 80.f;
static float g_minDamage  = 10.f;

static int   g_hitboxSel = 0;
static const char* g_hitboxes[] = { "head, chest", "head only", "body", "legs", "random" };

static float g_listHover[4] = {0};
static float g_sideHover[7] = {0};

// ============================================================
//  STYLE
// ============================================================
void UI::ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 0.f;
    s.ChildRounding      = 8.f;
    s.FrameRounding      = 6.f;
    s.PopupRounding      = 8.f;
    s.ScrollbarRounding  = 8.f;
    s.GrabRounding       = 6.f;
    s.WindowBorderSize   = 0.f;
    s.FrameBorderSize    = 0.f;
    s.PopupBorderSize    = 1.f;
    s.WindowPadding      = ImVec2(0,0);
    s.FramePadding       = ImVec2(10, 5);
    s.ItemSpacing        = ImVec2(8, 6);
    s.ItemInnerSpacing   = ImVec2(6, 4);
    s.ScrollbarSize      = 6.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.933f,0.925f,0.902f,1.f);
    c[ImGuiCol_ChildBg]         = ImVec4(0,0,0,0);
    c[ImGuiCol_PopupBg]         = ImVec4(1.f,1.f,1.f,0.98f);
    c[ImGuiCol_Border]          = ImVec4(0,0,0,0.08f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.95f,0.94f,0.92f,1.f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.90f,0.89f,0.86f,1.f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.85f,0.84f,0.81f,1.f);
    c[ImGuiCol_Button]          = ImVec4(0.94f,0.93f,0.91f,1.f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.88f,0.87f,0.84f,1.f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.80f,0.79f,0.76f,1.f);
    c[ImGuiCol_Header]          = ImVec4(0.87f,0.86f,0.83f,1.f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.82f,0.81f,0.78f,1.f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.75f,0.74f,0.71f,1.f);
    c[ImGuiCol_Separator]       = ImVec4(0,0,0,0.08f);
    c[ImGuiCol_Text]            = ImVec4(0.12f,0.11f,0.09f,1.f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.47f,0.45f,0.42f,1.f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.10f,0.10f,0.10f,1.f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.10f,0.10f,0.10f,1.f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.25f,0.25f,0.25f,1.f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0,0,0,0.15f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0,0,0,0.25f);
    c[ImGuiCol_ModalWindowDimBg]= ImVec4(0,0,0,0.35f);
}

// ---- Textures ----
static void ensureLogo() {
    if (g_logoTried) return; g_logoTried = true;
    if (RAVENXD_LOGO_PNG_SIZE > 4)
        LoadTextureFromMemory(RAVENXD_LOGO_PNG, RAVENXD_LOGO_PNG_SIZE, &g_logoTex);
}
static void ensureBg() {
    if (g_bgTried) return; g_bgTried = true;
    if (RAVENXD_BG_PNG_SIZE > 4)
        LoadTextureFromMemory(RAVENXD_BG_PNG, RAVENXD_BG_PNG_SIZE, &g_bgTex);
}
static void ensureBanner() {
    if (g_bannerTried) return; g_bannerTried = true;
    if (RAVENXD_BANNER_PNG_SIZE > 4)
        LoadTextureFromMemory(RAVENXD_BANNER_PNG, RAVENXD_BANNER_PNG_SIZE, &g_bannerTex);
}

// ---- Card ----
static void Card(ImDrawList* dl, ImVec2 a, ImVec2 b, float r = 8.f,
                 ImU32 base = K_CARD)
{
    dl->AddRectFilled(a, b, base, r);
    dl->AddRect(a, b, K_STROKE, r, 0, 1.f);
}

// ============================================================
//  CHECKBOX với 16 FX
// ============================================================
static bool KCheckbox(const char* label, bool* v)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    const float boxW = 14.f, boxH = 14.f, rowH = 22.f;
    float textW = ImGui::CalcTextSize(label).x;
    float totalW = boxW + 8.f + textW;

    ImGui::InvisibleButton(label, ImVec2(totalW, rowH));
    bool hov = ImGui::IsItemHovered();
    bool clk = ImGui::IsItemClicked();

    int fxId = (int)(ImGui::GetID(label) & 0x7FFFFFFF);

    FX::TickUpdate(fxId, *v, g_dt);
    FX::UpdateParticles(fxId, g_dt);

    float shakeX = FX::GetShake(fxId);
    float squash = FX::GetSquash(fxId);
    float sBoxW = boxW * squash;
    float sBoxH = boxH * squash;

    ImVec2 b0(pos.x + shakeX + (boxW - sBoxW)*0.5f,
              pos.y + (rowH - sBoxH)*0.5f);
    ImVec2 b1(b0.x + sBoxW, b0.y + sBoxH);

    if (clk) {
        *v = !*v;
        ImVec2 center((b0.x+b1.x)*0.5f, (b0.y+b1.y)*0.5f);
        FX::OnCheckChanged(fxId, *v, center);
    }

    // Hover trail
    if (hov) {
        auto& f = FX::Get(fxId);
        static ImVec2 lastMouse = ImGui::GetIO().MousePos;
        ImVec2 mp = ImGui::GetIO().MousePos;
        float speed = fabsf(mp.x - lastMouse.x) + fabsf(mp.y - lastMouse.y);
        if (speed > 8.f) {
            f.trailX = mp.x;
            f.trailT = FX::g_now;
            // Particle trail dài
            f.trailPts[f.trailHead].x = mp.x;
            f.trailPts[f.trailHead].y = mp.y;
            f.trailPts[f.trailHead].life = 1.f;
            f.trailHead = (f.trailHead + 1) % FX::WidgetFX::MAX_TRAIL;
        }
        lastMouse = mp;
    }

    ImU32 bg = *v ? K_ACTIVE : (hov ? K_HOVER : K_CARD_ALT);
    ImU32 border = *v ? K_ACTIVE : (hov ? K_STROKE_HI : K_STROKE);
    dl->AddRectFilled(b0, b1, bg, 3.f);
    dl->AddRect(b0, b1, border, 3.f, 0, 1.f);

    // Animated tick
    float tickAmt = FX::Get(fxId).tickAnim;
    if (tickAmt > 0.01f) {
        ImVec2 c((b0.x+b1.x)*0.5f, (b0.y+b1.y)*0.5f);
        float sc = FX::EaseOutBack(tickAmt);
        ImU32 tickCol = IM_COL32(255,255,255,(int)(255*tickAmt));
        dl->AddLine(ImVec2(c.x - 3.5f*sc, c.y),
                    ImVec2(c.x - 1.f*sc, c.y + 2.5f*sc), tickCol, 1.8f);
        dl->AddLine(ImVec2(c.x - 1.f*sc, c.y + 2.5f*sc),
                    ImVec2(c.x + 3.5f*sc, c.y - 2.5f*sc), tickCol, 1.8f);
    }

    // Box FX
    FX::DrawFX(dl, fxId, b0, b1);

    // Particle trail dài
    {
        auto& f = FX::Get(fxId);
        for (int i = 0; i < FX::WidgetFX::MAX_TRAIL; ++i) {
            auto& tp = f.trailPts[i];
            if (tp.life <= 0.f) continue;
            int a = (int)(200 * tp.life);
            dl->AddCircleFilled(ImVec2(tp.x, tp.y), 2.f * tp.life,
                                IM_COL32(80,190,120,a));
        }
    }

    // Label
    dl->AddText(ImVec2(b1.x + 8.f,
                       pos.y + (rowH - ImGui::GetTextLineHeight())*0.5f),
                K_TEXT, label);

    // Particles (confetti + pops)
    FX::DrawParticles(dl, fxId);

    return clk;
}

// ============================================================
//  SLIDER với number popup + sound
// ============================================================
static bool KSlider(const char* label, float* v, float min, float max,
                    const char* fmt = "%.0f")
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    const float rowH = 24.f;
    const float trackH = 4.f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const float labelW = 100.f;
    const float valueW = 60.f;

    int fxId = (int)(ImGui::GetID(label) & 0x7FFFFFFF);
    auto& f = FX::Get(fxId);

    // Label
    dl->AddText(ImVec2(pos.x, pos.y + (rowH - ImGui::GetTextLineHeight())*0.5f),
                K_TEXT, label);

    // Value
    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), fmt, *v);
    ImVec2 vts = ImGui::CalcTextSize(vbuf);
    dl->AddText(ImVec2(pos.x + availW - vts.x, pos.y + (rowH - vts.y)*0.5f),
                K_TEXT_DIM, vbuf);

    // Track
    float trackX = pos.x + labelW;
    float trackW = availW - labelW - valueW - 8.f;
    float trackY = pos.y + rowH*0.5f;

    dl->AddRectFilled(ImVec2(trackX, trackY - trackH*0.5f),
                      ImVec2(trackX + trackW, trackY + trackH*0.5f),
                      U32(210,208,200), trackH*0.5f);

    float t = (*v - min) / (max - min);
    if (t < 0) t = 0; if (t > 1) t = 1;
    float fillW = trackW * t;
    dl->AddRectFilled(ImVec2(trackX, trackY - trackH*0.5f),
                      ImVec2(trackX + fillW, trackY + trackH*0.5f),
                      K_ACTIVE, trackH*0.5f);

    float kx = trackX + fillW;
    float knobR = 6.f + f.glowAmt * 2.f;
    dl->AddCircleFilled(ImVec2(kx, trackY), knobR, K_ACTIVE);
    dl->AddCircle(ImVec2(kx, trackY), knobR, K_STROKE_HI, 0, 1.f);

    if (f.glowAmt > 0.01f) {
        int a = (int)(120 * f.glowAmt);
        dl->AddCircle(ImVec2(kx, trackY), knobR + 4.f,
                      IM_COL32(80,190,120,a), 0, 2.f);
    }

    // Interaction
    ImGui::SetCursorScreenPos(ImVec2(trackX, pos.y));
    ImGui::InvisibleButton(label, ImVec2(trackW, rowH));
    bool changed = false;
    static bool wasActive = false;
    bool isActive = ImGui::IsItemActive();

    if (isActive && !wasActive) {
        FX::OnSliderDragStart(fxId);
    }
    wasActive = isActive;

    if (isActive) {
        float mx = ImGui::GetIO().MousePos.x;
        float nt = (mx - trackX) / trackW;
        if (nt < 0) nt = 0; if (nt > 1) nt = 1;
        *v = min + nt * (max - min);
        changed = true;
        f.glowAmt = 1.f;
        // Number popup
        FX::OnSliderChanged(fxId, *v, ImVec2(kx, trackY - 20.f), fmt);
    }
    f.glowAmt *= expf(-3.f * g_dt);

    FX::UpdateParticles(fxId, g_dt);
    FX::DrawParticles(dl, fxId);

    return changed;
}

// ============================================================
//  SIDEBAR
// ============================================================
static void DrawSidebar(ImDrawList* dl, ImVec2 pos, ImVec2 size, int* selected)
{
    static const char* items[] = {
        "Ragebot", "Legitbot", "Players", "World", "Skins", "Misc", "Config"
    };
    const int count = 7;

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), K_SIDEBAR, 0.f);
    dl->AddLine(ImVec2(pos.x + size.x - 0.5f, pos.y),
                ImVec2(pos.x + size.x - 0.5f, pos.y + size.y), K_STROKE, 1.f);

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x + 18, pos.y + 18), K_TEXT, "kaminari");
    ImGui::PopFont();

    float y = pos.y + 70.f;
    const float itemH = 32.f;

    for (int i = 0; i < count; ++i) {
        ImVec2 ip(pos.x + 8.f, y);
        ImVec2 isz(size.x - 16.f, itemH);

        ImGui::SetCursorScreenPos(ip);
        ImGui::PushID(1000 + i);
        ImGui::InvisibleButton("##side", isz);
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_sideHover[i] = ExpSmooth(g_sideHover[i], hov ? 1.f : 0.f, 18.f);
        bool active = (*selected == i);

        if (active)
            dl->AddRectFilled(ip, ImVec2(ip.x + isz.x, ip.y + isz.y), K_CARD, 6.f);
        else if (g_sideHover[i] > 0.01f)
            dl->AddRectFilled(ip, ImVec2(ip.x + isz.x, ip.y + isz.y),
                              U32(0,0,0,(int)(15 * g_sideHover[i])), 6.f);

        ImVec2 ico(ip.x + 16.f, ip.y + itemH*0.5f);
        ImU32 iconCol = active ? K_TEXT : K_TEXT_DIM;
        dl->AddRect(ImVec2(ico.x-5, ico.y-5), ImVec2(ico.x+5, ico.y+5),
                    iconCol, 2.f, 0, 1.5f);

        dl->AddText(ImVec2(ip.x + 34.f,
                           ip.y + (itemH - ImGui::GetTextLineHeight())*0.5f),
                    active ? K_TEXT : K_TEXT_DIM, items[i]);

        if (clk) {
            *selected = i;
            FX::PlayFXSound(FX::SND_CLICK);
            FX::TriggerCursorPulse();
        }
        y += itemH + 2.f;
    }
}

// ============================================================
//  TOP TABS
// ============================================================
static void DrawTopTabs(ImDrawList* dl, ImVec2 pos, ImVec2 size, int* sel)
{
    static const char* tabs[] = { "pistol", "smg", "rifle", "shotgun", "sniper", "lmg" };
    const int count = 6;
    const float tabH = 26.f;
    const float padX = 14.f;

    float tabW[6];
    for (int i = 0; i < count; ++i)
        tabW[i] = ImGui::CalcTextSize(tabs[i]).x + padX*2.f;

    float x = pos.x;
    float y = pos.y + (size.y - tabH) * 0.5f;

    float targetX = pos.x;
    for (int i = 0; i < *sel; ++i) targetX += tabW[i] + 6.f;
    if (g_tabSubX < 0.f) g_tabSubX = targetX;
    g_tabSubX = ExpSmooth(g_tabSubX, targetX, 16.f);

    dl->AddRectFilled(ImVec2(g_tabSubX, y),
                      ImVec2(g_tabSubX + tabW[*sel], y + tabH), K_ACTIVE, 6.f);

    for (int i = 0; i < count; ++i) {
        ImVec2 tp(x, y);
        ImGui::SetCursorScreenPos(tp);
        ImGui::PushID(2000 + i);
        ImGui::InvisibleButton("##tab", ImVec2(tabW[i], tabH));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_tabSubHover[i] = ExpSmooth(g_tabSubHover[i], hov ? 1.f : 0.f, 18.f);
        bool active = (*sel == i);

        ImU32 textCol = active ? U32(255,255,255)
                                : U32(80,76,70,(int)(180 + 60*g_tabSubHover[i]));
        ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
        dl->AddText(ImVec2(tp.x + (tabW[i] - ts.x)*0.5f,
                           tp.y + (tabH - ts.y)*0.5f), textCol, tabs[i]);

        if (clk) { *sel = i; FX::PlayFXSound(FX::SND_CLICK); }
        x += tabW[i] + 6.f;
    }

    ImVec2 sp(pos.x + size.x - 24.f, pos.y + size.y*0.5f);
    dl->AddCircle(sp, 7.f, K_TEXT_DIM, 0, 1.5f);
    dl->AddLine(ImVec2(sp.x + 5.f, sp.y + 5.f),
                ImVec2(sp.x + 9.f, sp.y + 9.f), K_TEXT_DIM, 1.5f);
}

// ============================================================
//  RIGHT LIST
// ============================================================
static void DrawRightList(ImDrawList* dl, ImVec2 pos, ImVec2 size)
{
    static const char* items[] = { "zeusbot", "knif ebot", "quick peek", "duck peek" };
    const int count = 4;
    const float itemH = 38.f;
    const float gap = 6.f;

    float y = pos.y;
    for (int i = 0; i < count; ++i) {
        ImVec2 ip(pos.x, y);
        ImVec2 isz(size.x, itemH);

        ImGui::SetCursorScreenPos(ip);
        ImGui::PushID(3000 + i);
        ImGui::InvisibleButton("##ritem", isz);
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_listHover[i] = ExpSmooth(g_listHover[i], hov ? 1.f : 0.f, 18.f);

        ImU32 bg = g_listHover[i] > 0.01f
            ? ImGui::GetColorU32(ImLerp(ImGui::ColorConvertU32ToFloat4(K_CARD),
                                        ImGui::ColorConvertU32ToFloat4(K_CARD_ALT),
                                        g_listHover[i]))
            : K_CARD;
        Card(dl, ip, ImVec2(ip.x + isz.x, ip.y + isz.y), 8.f, bg);

        ImVec2 sq(ip.x + 14, ip.y + itemH*0.5f - 5);
        dl->AddRect(sq, ImVec2(sq.x + 10, sq.y + 10), K_TEXT_DIM, 2.f, 0, 1.3f);

        dl->AddText(ImVec2(ip.x + 34.f,
                           ip.y + (itemH - ImGui::GetTextLineHeight())*0.5f),
                    K_TEXT, items[i]);

        ImVec2 dots(ip.x + isz.x - 18.f, ip.y + itemH*0.5f);
        for (int d = 0; d < 3; ++d)
            dl->AddCircleFilled(ImVec2(dots.x + (d-1)*4.f, dots.y), 1.5f, K_TEXT_DIM);

        if (clk) FX::PlayFXSound(FX::SND_CLICK);
        y += itemH + gap;
    }
}

// ============================================================
//  HITBOXES DROPDOWN
// ============================================================
static void DrawHitboxes(ImDrawList* dl, ImVec2 pos, ImVec2 size, int* sel)
{
    ImVec2 dp(pos.x, pos.y);
    ImVec2 dsz(size.x, 34.f);
    Card(dl, dp, ImVec2(dp.x + dsz.x, dp.y + dsz.y), 8.f, K_CARD);

    dl->AddText(ImVec2(dp.x + 14, dp.y + (dsz.y - ImGui::GetTextLineHeight())*0.5f),
                K_TEXT_DIM, "hitboxes");

    ImVec2 ts = ImGui::CalcTextSize(g_hitboxes[*sel]);
    dl->AddText(ImVec2(dp.x + dsz.x - ts.x - 34.f,
                       dp.y + (dsz.y - ts.y)*0.5f), K_TEXT, g_hitboxes[*sel]);

    ImVec2 cv(dp.x + dsz.x - 20.f, dp.y + dsz.y*0.5f);
    dl->AddLine(ImVec2(cv.x-4, cv.y-2), ImVec2(cv.x, cv.y+3), K_TEXT_DIM, 1.5f);
    dl->AddLine(ImVec2(cv.x, cv.y+3), ImVec2(cv.x+4, cv.y-2), K_TEXT_DIM, 1.5f);

    ImGui::SetCursorScreenPos(dp);
    ImGui::InvisibleButton("##hitbox", dsz);
    if (ImGui::IsItemClicked()) {
        *sel = (*sel + 1) % 5;
        FX::PlayFXSound(FX::SND_CLICK);
    }
}

// ============================================================
//  RAGEBOT PAGE
// ============================================================
static void DrawRagebotPage(ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 24.f;
    float contentY = pos.y + 60.f;
    float leftW = (size.x - pad*3) * 0.5f;
    float rightW = (size.x - pad*3) * 0.5f;

    ImVec2 lA(pos.x + pad, contentY);

    // Card 1: checkboxes
    float chkH = 5 * 24.f + 24.f;
    ImVec2 c1A = lA;
    ImVec2 c1B(lA.x + leftW, lA.y + chkH);
    Card(dl, c1A, c1B, 10.f, K_CARD);

    ImGui::SetCursorScreenPos(ImVec2(c1A.x + 16, c1A.y + 12));
    KCheckbox("enabled",        &g_check_enabled);
    ImGui::SetCursorScreenPos(ImVec2(c1A.x + 16, c1A.y + 36));
    KCheckbox("non auto fire",  &g_check_nonAuto);
    ImGui::SetCursorScreenPos(ImVec2(c1A.x + 16, c1A.y + 60));
    KCheckbox("silent",         &g_check_silent);
    ImGui::SetCursorScreenPos(ImVec2(c1A.x + 16, c1A.y + 84));
    KCheckbox("no spread",      &g_check_noSpread);
    ImGui::SetCursorScreenPos(ImVec2(c1A.x + 16, c1A.y + 108));
    KCheckbox("extrapolation",  &g_check_extrap);

    // Card 2: sliders
    float slY = c1B.y + 12.f;
    float slH = 3 * 32.f + 24.f;
    ImVec2 c2A(lA.x, slY);
    ImVec2 c2B(lA.x + leftW, slY + slH);
    Card(dl, c2A, c2B, 10.f, K_CARD);

    ImGui::SetCursorScreenPos(ImVec2(c2A.x + 16, c2A.y + 12));
    KSlider("max fov",    &g_fov,       0, 360, "%.0f");
    ImGui::SetCursorScreenPos(ImVec2(c2A.x + 16, c2A.y + 44));
    KSlider("hit chance", &g_hitChance, 0, 100, "%.0f%%");
    ImGui::SetCursorScreenPos(ImVec2(c2A.x + 16, c2A.y + 76));
    KSlider("min damage", &g_minDamage, 0, 100, "%.0f");

    // Hitboxes
    float hbY = c2B.y + 12.f;
    DrawHitboxes(dl, ImVec2(lA.x, hbY), ImVec2(leftW, 34.f), &g_hitboxSel);

    // Right list
    ImVec2 rA(pos.x + pad*2 + leftW, contentY);
    DrawRightList(dl, rA, ImVec2(rightW, size.y - 60.f - pad*2));
}

// ============================================================
//  SIMPLE PAGE
// ============================================================
static void DrawSimplePage(ImDrawList* dl, ImVec2 pos, ImVec2 size, const char* title)
{
    float pad = 24.f;
    ImVec2 cA(pos.x + pad, pos.y + 60.f);
    ImVec2 cB(pos.x + size.x - pad, pos.y + size.y - pad);
    Card(dl, cA, cB, 10.f, K_CARD);

    ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(cA.x + 24, cA.y + 24), K_TEXT, title);
    ImGui::PopFont();
    dl->AddText(ImVec2(cA.x + 24, cA.y + 52), K_TEXT_DIM, "Coming soon...");
}

// ============================================================
//  JAVA POPUP
// ============================================================
static void DrawJavaPopup(Launcher& L)
{
    if (L.s().showJavaPopup) ImGui::OpenPopup("Java");
    ImGui::SetNextWindowSize(ImVec2(420, 0));
    if (ImGui::BeginPopupModal("Java", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(g_fontBold);
        ImGui::TextColored(ImVec4(0.8f,0.2f,0.2f,1.f), "Java not found");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::TextWrapped("RavenXD couldn't locate a Java 8 installation. "
                           "Pick javaw.exe manually or install Java 8 (Temurin).");
        ImGui::Dummy(ImVec2(0, 8));
        if (ImGui::Button("Select Java", ImVec2(150, 32))) {
            char f[MAX_PATH] = {};
            OPENFILENAMEA o{};
            o.lStructSize = sizeof(o);
            o.hwndOwner = GetActiveWindow();
            o.lpstrFilter = "Java\0javaw.exe;java.exe\0All\0*.*\0";
            o.lpstrFile = f;
            o.nMaxFile = MAX_PATH;
            o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameA(&o)) {
                Settings::I().d().javaPath = f;
                Settings::I().save();
                L.s().javaOk = true;
                L.s().showJavaPopup = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Get Java 8", ImVec2(110, 32)))
            ShellExecuteA(nullptr, "open",
                "https://adoptium.net/temurin/releases/?version=8",
                nullptr, nullptr, SW_SHOW);
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(70, 32))) {
            L.s().showJavaPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================
//  LOADING
// ============================================================
void UI::RenderLoadingScreen(Launcher& L)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, K_BG);

    float t = L.s().loadingTimer;
    float a = t < 0.6f ? t/0.6f : 1.f;
    float cx = io.DisplaySize.x*0.5f, cy = io.DisplaySize.y*0.5f;

    ImGui::PushFont(g_fontBig);
    ImGui::SetCursorScreenPos(ImVec2(cx-60, cy-16));
    ImGui::TextColored(ImVec4(0.12f,0.11f,0.09f,a), "kaminari");
    ImGui::PopFont();

    const char* ph = t < 0.8f ? "Loading..." : "Ready";
    ImVec2 ts = ImGui::CalcTextSize(ph);
    dl->AddText(ImVec2(cx-ts.x*0.5f, cy+30.f), U32(120,116,108,(int)(a*255)), ph);
}

// ============================================================
//  ROOT
// ============================================================
void UI::Render(Launcher& L, float dt)
{
    g_dt = dt > 0.f ? dt : (1.f/60.f);
    g_timeNow += g_dt;
    FX::BeginFrame(g_dt);
    L.tick(dt);

    if (L.s().showLoadingScreen) { RenderLoadingScreen(L); return; }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Screen shake offset
    ImVec2 shake = FX::GetScreenShake();

    dl->AddRectFilled(ImVec2(shake.x, shake.y),
                      ImVec2(io.DisplaySize.x + shake.x, io.DisplaySize.y + shake.y),
                      K_BG);

    const float sidebarW = 180.f;
    static int sideSel = 0;

    DrawSidebar(dl,
                ImVec2(shake.x, shake.y),
                ImVec2(sidebarW, io.DisplaySize.y),
                &sideSel);

    ImGui::SetNextWindowPos(ImVec2(sidebarW + shake.x, shake.y));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - sidebarW, io.DisplaySize.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##content", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    ImVec2 cp(24.f, 20.f);

    ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(cp.x, cp.y), K_TEXT, "Ragebot");
    ImGui::PopFont();

    DrawTopTabs(dl, ImVec2(cp.x, cp.y + 30.f),
                ImVec2(io.DisplaySize.x - sidebarW - cp.x*2, 26.f), &g_tabSub);

    ImVec2 pagePos(sidebarW, 0);
    ImVec2 pageSize(io.DisplaySize.x - sidebarW, io.DisplaySize.y);

    if (sideSel == 0) {
        DrawRagebotPage(pagePos, pageSize);
    } else {
        static const char* names[] = { "", "Legitbot", "Players", "World",
                                       "Skins", "Misc", "Config" };
        DrawSimplePage(dl, pagePos, pageSize, names[sideSel]);
    }

    ImGui::End();
    ImGui::PopStyleVar();

    // Cursor pulse
    FX::DrawCursorPulse(dl);

    DrawJavaPopup(L);
}
