#include "UI.h"
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

// ============================================================
//  EXTERNS (từ main.cpp)
// ============================================================
extern ID3D11Device*        g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern ImFont*              g_fontRegular;
extern ImFont*              g_fontBold;
extern ImFont*              g_fontBig;
extern void LoadTextureFromMemory(const unsigned char*, size_t,
                                  ID3D11ShaderResourceView**);

// ============================================================
//  TEXTURES
// ============================================================
ID3D11ShaderResourceView*        g_logoTex   = nullptr;
static ID3D11ShaderResourceView* g_bgTex     = nullptr;
static ID3D11ShaderResourceView* g_bannerTex = nullptr;
static bool g_logoTried    = false;
static bool g_bgTried      = false;
static bool g_bannerTried  = false;

// ============================================================
//  BLUE THEME PALETTE
// ============================================================
static inline ImU32 U32(int r,int g,int b,int a=255){ return IM_COL32(r,g,b,a); }

static const ImU32 C_BG        = U32( 15, 23, 42);
static const ImU32 C_SIDEBAR   = U32( 20, 30, 55);
static const ImU32 C_CARD      = U32( 30, 41, 70);
static const ImU32 C_CARD_HOV  = U32( 40, 55, 92);
static const ImU32 C_STROKE    = U32(255,255,255, 20);
static const ImU32 C_STROKE_HI = U32(255,255,255, 45);

static const ImU32 C_TEXT      = U32(241,245,249);
static const ImU32 C_TEXT_DIM  = U32(148,163,184);
static const ImU32 C_TEXT_DIM2 = U32(100,116,139);

static const ImU32 C_ACCENT    = U32( 59,130,246);
static const ImU32 C_ACCENT_HI = U32( 96,165,250);
static const ImU32 C_ACCENT_LO = U32( 37, 99,235);
static const ImU32 C_GREEN     = U32( 34,197, 94);
static const ImU32 C_RED       = U32(239, 68, 68);
static const ImU32 C_YELLOW    = U32(250,204, 21);

// ============================================================
//  STATE
// ============================================================
static char  g_newAccBuf[64] = "";
static float g_dt      = 1.f/60.f;
static float g_timeNow = 0.f;

static inline float ExpSmooth(float cur, float tgt, float sp) {
    float k = 1.f - expf(-sp * g_dt);
    return cur + (tgt - cur) * k;
}
static inline float EaseOutCubic(float t) { return 1.f - powf(1.f-t, 3.f); }

struct SpringBtn { float pressT = -1.f; float scale = 1.f; bool wasDown = false; };
static SpringBtn g_spring[32];

struct Ripple { ImVec2 pos; float startTime = -1.f; float duration = 0.55f; bool active = false; };
static const int MAX_RIPPLES = 8;
static Ripple g_ripples[MAX_RIPPLES];
static int    g_rippleIdx = 0;

static float g_sideHover[4]  = {0,0,0,0};
static float g_sidePillY     = -1.f;
static float g_toggleAnim[8] = {0};
static float g_launchPulse   = 0.f;
static float g_dcHover       = 0.f;

static int   g_prevPage = -1;
static float g_pageAnim = 1.f;

// ============================================================
//  CLICK EFFECT (spring + ripple)
// ============================================================
static float SpringScale(float pressT, float now)
{
    if (pressT < 0.f) return 1.f;
    float t = now - pressT;
    if (t < 0.f || t > 0.45f) return 1.f;
    const float k = 180.f, d = 14.f;
    float decay = expf(-d * t);
    float osc   = cosf(sqrtf(k) * t);
    return 1.f + 0.12f * decay * (osc * -1.f);
}

static void SpawnRipple(ImVec2 screenPos)
{
    g_ripples[g_rippleIdx].pos       = screenPos;
    g_ripples[g_rippleIdx].startTime = g_timeNow;
    g_ripples[g_rippleIdx].active    = true;
    g_rippleIdx = (g_rippleIdx + 1) % MAX_RIPPLES;
}

static void DrawRipples(ImDrawList* dl)
{
    for (int i = 0; i < MAX_RIPPLES; ++i) {
        auto& r = g_ripples[i];
        if (!r.active) continue;
        float t = g_timeNow - r.startTime;
        if (t < 0.f || t > r.duration) { r.active = false; continue; }
        float p   = t / r.duration;
        float rad = 8.f + 70.f * (1.f - powf(1.f - p, 2.f));
        int   a   = (int)(140 * (1.f - p) * (1.f - p));
        if (a <= 0) { r.active = false; continue; }
        dl->AddCircleFilled(r.pos, rad, U32(96,165,250, a / 3));
        dl->AddCircle(r.pos, rad + 3.f, U32(147,197,253, a), 0, 2.f);
    }
}

static float ClickEffect(int slotId, bool hovered, bool clicked, ImVec2 center)
{
    if (slotId < 0 || slotId >= 32) return 1.f;
    auto& s = g_spring[slotId];

    bool down = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (down && !s.wasDown) s.pressT = g_timeNow;
    s.wasDown = down;

    if (clicked) SpawnRipple(center);

    s.scale = SpringScale(s.pressT, g_timeNow);

    if (s.pressT > 0.f && g_timeNow - s.pressT < 0.25f) {
        float t = (g_timeNow - s.pressT) / 0.25f;
        int a = (int)(90 * (1.f - t));
        ImGui::GetWindowDrawList()->AddCircleFilled(
            center, 34.f * (1.f + t), U32(96,165,250, a));
    }
    return s.scale;
}

// ============================================================
//  STYLE
// ============================================================
void UI::ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 12.f;
    s.ChildRounding      = 12.f;
    s.FrameRounding      = 10.f;
    s.PopupRounding      = 12.f;
    s.ScrollbarRounding  = 10.f;
    s.GrabRounding       = 10.f;
    s.TabRounding        = 10.f;
    s.WindowBorderSize   = 0.f;
    s.FrameBorderSize    = 0.f;
    s.PopupBorderSize    = 1.f;
    s.WindowPadding      = ImVec2(0,0);
    s.FramePadding       = ImVec2(12, 6);
    s.ItemSpacing        = ImVec2(10, 8);
    s.ItemInnerSpacing   = ImVec2(6, 4);
    s.ScrollbarSize      = 8.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.06f,0.09f,0.17f,1.f);
    c[ImGuiCol_ChildBg]         = ImVec4(0,0,0,0);
    c[ImGuiCol_PopupBg]         = ImVec4(0.10f,0.14f,0.24f,0.98f);
    c[ImGuiCol_Border]          = ImVec4(1,1,1,0.08f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.12f,0.16f,0.27f,1.f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.16f,0.21f,0.36f,1.f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.20f,0.27f,0.44f,1.f);
    c[ImGuiCol_Button]          = ImVec4(0.15f,0.20f,0.35f,1.f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.23f,0.32f,0.55f,1.f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.30f,0.42f,0.70f,1.f);
    c[ImGuiCol_Header]          = ImVec4(0.23f,0.51f,0.96f,0.7f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.23f,0.51f,0.96f,0.85f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.23f,0.51f,0.96f,1.f);
    c[ImGuiCol_Separator]       = ImVec4(1,1,1,0.08f);
    c[ImGuiCol_Text]            = ImVec4(0.95f,0.96f,0.98f,1.f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.60f,0.64f,0.72f,1.f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.23f,0.51f,0.96f,1.f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.23f,0.51f,0.96f,1.f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.38f,0.65f,1.f,1.f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(1,1,1,0.18f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1,1,1,0.30f);
    c[ImGuiCol_ModalWindowDimBg]= ImVec4(0,0,0,0.55f);
}

// ============================================================
//  TEXTURE LOADERS
// ============================================================
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

// ============================================================
//  DRAW HELPERS
// ============================================================
static void GradV(ImDrawList* dl, ImVec2 a, ImVec2 b,
                  ImU32 top, ImU32 bot, float r)
{
    ImVec4 cT = ImGui::ColorConvertU32ToFloat4(top);
    ImVec4 cB = ImGui::ColorConvertU32ToFloat4(bot);
    const int N = 24;
    float h = b.y - a.y;
    for (int i = 0; i < N; ++i) {
        float t = (float)i / (N-1);
        ImU32 c = ImGui::GetColorU32(ImLerp(cT, cB, t));
        float y0 = a.y + h * (i   /(float)N);
        float y1 = a.y + h * ((i+1)/(float)N);
        ImDrawFlags f = 0;
        if (i==0)   f = ImDrawFlags_RoundCornersTop;
        if (i==N-1) f = ImDrawFlags_RoundCornersBottom;
        dl->AddRectFilled(ImVec2(a.x,y0), ImVec2(b.x,y1), c, f?r:0.f, f);
    }
}

static void Card(ImDrawList* dl, ImVec2 a, ImVec2 b, float r = 12.f,
                 ImU32 base = C_CARD)
{
    dl->AddRectFilled(a, b, base, r);
    dl->AddRect(a, b, C_STROKE, r, 0, 1.f);
}

// ============================================================
//  ICONS
// ============================================================
static void I_Home(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddTriangleFilled(ImVec2(c.x, c.y-s*0.55f),
        ImVec2(c.x-s*0.55f, c.y-s*0.05f), ImVec2(c.x+s*0.55f, c.y-s*0.05f), col);
    dl->AddRectFilled(ImVec2(c.x-s*0.10f, c.y-s*0.05f),
                      ImVec2(c.x+s*0.10f, c.y+s*0.55f), col, 1.f);
}
static void I_User(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x, c.y-s*0.18f), s*0.30f, col);
    dl->AddCircleFilled(ImVec2(c.x, c.y+s*0.52f), s*0.50f, col);
}
static void I_Gear(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(c, s*0.45f, col);
    dl->AddCircleFilled(c, s*0.20f, C_SIDEBAR);
    for (int i=0;i<8;++i){
        float a = i*IM_PI/4.f;
        dl->AddLine(ImVec2(c.x+cosf(a)*s*0.45f, c.y+sinf(a)*s*0.45f),
                    ImVec2(c.x+cosf(a)*s*0.68f, c.y+sinf(a)*s*0.68f), col, 2.2f);
    }
}
static void I_Puzzle(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x, c.y), s*0.45f, col);
    dl->AddCircleFilled(ImVec2(c.x, c.y), s*0.20f, C_SIDEBAR);
    for (int i=0;i<6;++i){
        float a = i*IM_PI/3.f;
        dl->AddLine(ImVec2(c.x+cosf(a)*s*0.45f, c.y+sinf(a)*s*0.45f),
                    ImVec2(c.x+cosf(a)*s*0.68f, c.y+sinf(a)*s*0.68f), col, 2.2f);
    }
}
static void I_Minus(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.35f, c.y), ImVec2(c.x+s*0.35f, c.y), col, 2.f);
}
static void I_Close(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.30f, c.y-s*0.30f),
                ImVec2(c.x+s*0.30f, c.y+s*0.30f), col, 2.f);
    dl->AddLine(ImVec2(c.x+s*0.30f, c.y-s*0.30f),
                ImVec2(c.x-s*0.30f, c.y+s*0.30f), col, 2.f);
}
static void I_Chev(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.20f, c.y-s*0.35f),
                ImVec2(c.x+s*0.20f, c.y), col, 2.f);
    dl->AddLine(ImVec2(c.x+s*0.20f, c.y),
                ImVec2(c.x-s*0.20f, c.y+s*0.35f), col, 2.f);
}

// ============================================================
//  TOP BAR
// ============================================================
static void DrawTopBar(Launcher& L, ImDrawList* dl, ImVec2 disp)
{
    (void)L;
    const float barH = 56.f;
    dl->AddRectFilled(ImVec2(0,0), ImVec2(disp.x, barH), C_BG, 0.f);
    dl->AddLine(ImVec2(0, barH-0.5f), ImVec2(disp.x, barH-0.5f), C_STROKE, 1.f);

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(24, barH*0.5f - 15.f), C_TEXT, "RAVENXD");
    ImGui::PopFont();

    // Language pill
    float langX = 200.f, langY = barH*0.5f - 14.f;
    ImVec2 langA(langX, langY), langB(langX + 130.f, langY + 28.f);
    Card(dl, langA, langB, 8.f, C_CARD);
    ImGui::SetCursorScreenPos(ImVec2(langA.x + 12.f, langA.y + 6.f));
    ImGui::TextUnformatted("English");
    I_Chev(dl, ImVec2(langB.x - 16.f, langA.y + 14.f), 7.f, C_TEXT);

    // Minimize + Close
    float r = 16.f;
    float closeX = disp.x - 22.f - r, closeY = barH*0.5f;

    ImGui::SetCursorScreenPos(ImVec2(closeX - r, closeY - r));
    ImGui::InvisibleButton("##close", ImVec2(r*2, r*2));
    bool closeHov = ImGui::IsItemHovered();
    bool closeClk = ImGui::IsItemClicked();
    if (closeHov) dl->AddCircleFilled(ImVec2(closeX, closeY), r, U32(220,50,50,150));
    I_Close(dl, ImVec2(closeX, closeY), 10.f, C_TEXT);

    float minX = closeX - r*2.f - 8.f;
    ImGui::SetCursorScreenPos(ImVec2(minX - r, closeY - r));
    ImGui::InvisibleButton("##min", ImVec2(r*2, r*2));
    bool minHov = ImGui::IsItemHovered();
    bool minClk = ImGui::IsItemClicked();
    if (minHov) dl->AddCircleFilled(ImVec2(minX, closeY), r, U32(255,255,255,25));
    I_Minus(dl, ImVec2(minX, closeY), 10.f, C_TEXT);

    if (closeClk) PostQuitMessage(0);
    if (minClk)   ShowWindow(GetActiveWindow(), SW_MINIMIZE);
}

// ============================================================
//  SIDEBAR (4 tabs)
// ============================================================
static void DrawSidebar(Launcher& L, ImDrawList* dl, ImVec2 pos, ImVec2 size)
{
    struct Row { int page; void(*icon)(ImDrawList*,ImVec2,float,ImU32); };
    static const Row rows[4] = {
        { (int)Page::Home,     I_Home },
        { (int)Page::Accounts, I_User },
        { (int)Page::Mods,     I_Puzzle },
        { (int)Page::Settings, I_Gear },
    };

    dl->AddRectFilled(pos, ImVec2(pos.x+size.x, pos.y+size.y), C_SIDEBAR, 0.f);
    dl->AddLine(ImVec2(pos.x+size.x-0.5f, pos.y),
                ImVec2(pos.x+size.x-0.5f, pos.y+size.y), C_STROKE, 1.f);

    const float btnSize = 52.f, gapY = 8.f, startY = pos.y + 30.f;
    float btnX = pos.x + (size.x - btnSize) * 0.5f;

    int cur = (int)L.s().page;
    int activeIdx = 0;
    for (int i = 0; i < 4; ++i)
        if (rows[i].page == cur) activeIdx = i;

    float targetPillY = startY + activeIdx * (btnSize + gapY);
    if (g_sidePillY < 0.f) g_sidePillY = targetPillY;
    g_sidePillY = ExpSmooth(g_sidePillY, targetPillY, 18.f);

    GradV(dl, ImVec2(btnX, g_sidePillY),
              ImVec2(btnX + btnSize, g_sidePillY + btnSize),
              C_ACCENT_HI, C_ACCENT_LO, 12.f);

    for (int i = 0; i < 4; ++i) {
        ImVec2 bp(btnX, startY + i*(btnSize + gapY));
        ImGui::SetCursorScreenPos(bp);
        ImGui::PushID(3000 + i);
        ImGui::InvisibleButton("##side", ImVec2(btnSize, btnSize));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_sideHover[i] = ExpSmooth(g_sideHover[i], hov ? 1.f : 0.f, 16.f);
        bool active = (rows[i].page == cur);

        ImVec2 center(bp.x + btnSize*0.5f, bp.y + btnSize*0.5f);
        float scale = ClickEffect(i, hov, clk, center);

        if (!active && g_sideHover[i] > 0.01f) {
            float sz = btnSize * scale;
            dl->AddRectFilled(ImVec2(center.x - sz*0.5f, center.y - sz*0.5f),
                              ImVec2(center.x + sz*0.5f, center.y + sz*0.5f),
                              U32(255,255,255,(int)(20 * g_sideHover[i])), 12.f);
        }

        ImU32 iconCol = active ? U32(255,255,255) : C_TEXT_DIM;
        rows[i].icon(dl, center, 14.f * scale, iconCol);

        if (clk && !active) L.s().page = (Page)rows[i].page;
    }
}

// ============================================================
//  HOME PAGE
// ============================================================
static void DrawHomePage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float gap = 20.f;
    const float leftW = size.x * 0.42f;
    const float rightW = size.x - leftW - gap;

    ImVec2 leftA(pos.x, pos.y);
    ImVec2 leftB(pos.x + leftW, pos.y + size.y);
    ImVec2 rightA(pos.x + leftW + gap, pos.y);
    ImVec2 rightB(pos.x + size.x, pos.y + size.y);

    ensureBanner();
    if (g_bannerTex) {
        dl->AddImageRounded((ImTextureID)g_bannerTex, leftA, leftB,
                            ImVec2(0,0), ImVec2(1,1), U32(255,255,255), 16.f);
    } else if (g_bgTex) {
        dl->AddImageRounded((ImTextureID)g_bgTex, leftA, leftB,
                            ImVec2(0,0), ImVec2(1,1), U32(255,255,255), 16.f);
    } else {
        GradV(dl, leftA, leftB, C_ACCENT_HI, C_ACCENT_LO, 16.f);
    }

    Card(dl, rightA, rightB, 16.f, C_CARD);

    float pad = 28.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(rightA.x + pad, rightA.y + pad), C_TEXT, "BETA 1.8.9");
    ImGui::PopFont();

    // RETURN
    {
        const char* ret = "RETURN";
        ImVec2 rsz = ImGui::CalcTextSize(ret);
        ImVec2 rp(rightB.x - pad - rsz.x - 20.f, rightA.y + pad + 8.f);

        ImGui::SetCursorScreenPos(ImVec2(rp.x - 8.f, rp.y - 6.f));
        ImGui::InvisibleButton("##return", ImVec2(rsz.x + 28.f, rsz.y + 12.f));
        bool rHov = ImGui::IsItemHovered();
        (void)ImGui::IsItemClicked();

        ImU32 rcol = rHov ? C_TEXT : C_TEXT_DIM;
        dl->AddText(rp, rcol, ret);
        ImVec2 ac(rp.x + rsz.x + 12.f, rp.y + rsz.y*0.5f);
        dl->AddLine(ImVec2(ac.x-6, ac.y), ImVec2(ac.x+4, ac.y), rcol, 2.f);
        dl->AddTriangleFilled(ImVec2(ac.x+2, ac.y-3), ImVec2(ac.x+2, ac.y+3),
                              ImVec2(ac.x+7, ac.y), rcol);
    }

    float tagY = rightA.y + pad + 52.f;
    dl->AddText(ImVec2(rightA.x + pad, tagY), C_TEXT_DIM, "Client");

    float descY = tagY + 34.f;
    float descW = rightW - pad*2;
    const char* desc =
        "RavenXD la client Minecraft 1.8.9 toi uu cho PvP, "
        "tich hop Forge, OptiFine va nhieu mod ho tro. "
        "Phien ban dang trong giai doan beta - co the cap nhat "
        "thuong xuyen de cai thien trai nghiem cua ban.";

    // AddText với size cứng (không dùng g_fontRegular->FontSize)
    dl->AddText(g_fontRegular, 15.f,
                ImVec2(rightA.x + pad, descY), C_TEXT_DIM,
                desc, nullptr, descW);

    // LAUNCH button
    float launchW = 150.f, launchH = 42.f;
    ImVec2 lb(rightB.x - pad - launchW, rightB.y - pad - launchH);

    g_launchPulse += g_dt;
    float pulse = 0.5f + 0.5f*sinf(g_launchPulse*2.2f);
    bool busy = (L.s().taskState == TaskState::Running);
    int glowA = busy ? 20 : (int)(30 + 25*pulse);
    dl->AddRectFilled(ImVec2(lb.x-4,lb.y-4),
                      ImVec2(lb.x+launchW+4, lb.y+launchH+4),
                      U32(59,130,246,glowA), 12.f);

    ImGui::SetCursorScreenPos(lb);
    ImGui::InvisibleButton("##launch", ImVec2(launchW, launchH));
    bool lHov = ImGui::IsItemHovered();
    bool lClk = ImGui::IsItemClicked();

    ImVec2 lCenter(lb.x + launchW*0.5f, lb.y + launchH*0.5f);
    float lScale = ClickEffect(20, lHov, lClk, lCenter);

    float sw = launchW * lScale, sh = launchH * lScale;
    ImVec2 b0(lCenter.x - sw*0.5f, lCenter.y - sh*0.5f);
    ImVec2 b1(lCenter.x + sw*0.5f, lCenter.y + sh*0.5f);

    ImU32 top = lHov ? C_ACCENT_HI : C_ACCENT;
    ImU32 bot = lHov ? C_ACCENT    : C_ACCENT_LO;
    GradV(dl, b0, b1, top, bot, 10.f);

    ImVec2 playC(b0.x + 26.f, lCenter.y);
    dl->AddTriangleFilled(ImVec2(playC.x-5, playC.y-7),
                          ImVec2(playC.x-5, playC.y+7),
                          ImVec2(playC.x+7, playC.y), U32(255,255,255));

    ImGui::PushFont(g_fontBold);
    const char* btnText = busy ? "..." : "LAUNCH";
    ImVec2 ts = ImGui::CalcTextSize(btnText);
    dl->AddText(ImVec2(lCenter.x - ts.x*0.5f + 12.f, lCenter.y - ts.y*0.5f),
                U32(255,255,255), btnText);
    ImGui::PopFont();

    if (lClk && !busy) L.onLaunchClicked();
}

// ============================================================
//  PAGE: ACCOUNTS
// ============================================================
static void DrawAccountsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    (void)L;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 24.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y), C_TEXT, "Accounts");
    ImGui::PopFont();
    dl->AddText(ImVec2(pos.x, pos.y + 48.f), C_TEXT_DIM,
                "Add offline accounts and pick which one to launch with");

    float addY = pos.y + 88.f;
    ImVec2 a0(pos.x, addY), a1(pos.x+size.x, addY+60.f);
    Card(dl, a0, a1, 12.f, C_CARD);

    ImGui::SetCursorScreenPos(ImVec2(a0.x+16, a0.y+30));
    ImGui::SetNextItemWidth(260);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    ImGui::InputText("##newacc", g_newAccBuf, sizeof(g_newAccBuf),
                     ImGuiInputTextFlags_CharsNoBlank);
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(100, 0))) {
        if (strlen(g_newAccBuf) > 0) {
            AccountManager::I().add(g_newAccBuf);
            g_newAccBuf[0] = 0;
        }
    }

    float listY = addY + 72.f;
    ImGui::SetCursorScreenPos(ImVec2(pos.x, listY));
    ImGui::BeginChild("##acclist", ImVec2(size.x, size.y-(listY-pos.y)-pad), false);

    auto& list = AccountManager::I().list();
    for (size_t i=0;i<list.size();++i) {
        auto& acc = list[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 70.f;

        ImU32 bg = acc.active ? U32(29, 78,160) : C_CARD;
        Card(dl, cp
