#include "UI.h"
#include "../DiscordRPC.h"
#include "../AccountManager.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "embedded_assets.h"
#include <d3d11.h>
#include <windows.h>
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

ID3D11ShaderResourceView*        g_logoTex = nullptr;
static ID3D11ShaderResourceView* g_bgTex   = nullptr;
static bool g_logoTried = false, g_bgTried = false;

// ============================================================
//  macOS PALETTE
// ============================================================
static inline ImU32 U32(int r,int g,int b,int a=255){ return IM_COL32(r,g,b,a); }

// macOS System Colors (Dark Mode)
static const ImU32 MAC_WINDOW_BG     = U32( 30, 30, 32);      // #1E1E20
static const ImU32 MAC_SIDEBAR_BG    = U32( 38, 38, 42, 210); // vibrancy
static const ImU32 MAC_TOOLBAR_BG    = U32( 44, 44, 48, 220);
static const ImU32 MAC_CONTENT_BG    = U32( 26, 26, 28);
static const ImU32 MAC_CARD_BG       = U32( 44, 44, 48);
static const ImU32 MAC_CARD_BG_2     = U32( 52, 52, 58);
static const ImU32 MAC_CARD_HOVER    = U32( 58, 58, 64);
static const ImU32 MAC_STROKE        = U32(255,255,255, 20);
static const ImU32 MAC_STROKE_HI     = U32(255,255,255, 40);

static const ImU32 MAC_TEXT          = U32(245,245,247);      // Label
static const ImU32 MAC_TEXT_DIM      = U32(152,152,159);      // Secondary Label
static const ImU32 MAC_TEXT_DIM2     = U32( 99, 99,102);      // Tertiary
static const ImU32 MAC_ACCENT        = U32( 10,132,255);      // systemBlue
static const ImU32 MAC_ACCENT_HI     = U32( 64,156,255);
static const ImU32 MAC_GREEN         = U32( 48,209, 88);      // systemGreen
static const ImU32 MAC_RED           = U32(255, 69, 58);      // systemRed
static const ImU32 MAC_YELLOW        = U32(255,214, 10);      // systemYellow
static const ImU32 MAC_ORANGE        = U32(255,159, 10);

// Traffic light colors
static const ImU32 TL_RED    = U32(255, 95, 87);
static const ImU32 TL_YELLOW = U32(255,189, 46);
static const ImU32 TL_GREEN  = U32( 40,200, 64);

// ============================================================
//  STATE
// ============================================================
static char  g_newAccBuf[64] = "";
static float g_dt = 1.f/60.f;

static inline float ExpSmooth(float cur, float tgt, float sp) {
    float k = 1.f - expf(-sp * g_dt);
    return cur + (tgt - cur) * k;
}
static inline float EaseOutCubic(float t) { return 1.f - powf(1.f-t, 3.f); }

// Sidebar hover
static float g_sbHover[5]    = {0,0,0,0,0};  // Home, Accounts, Versions, Mods, Settings
static float g_sbPillY       = -1.f;         // active pill Y
static float g_toggleAnim[8] = {0};
static float g_launchPulse   = 0.f;
static float g_dcHover       = 0.f;
static float g_verHover[4]   = {0,0,0,0};

// Traffic light hover
static float g_tlHover[3]    = {0,0,0};
static bool  g_tlGroupHover  = false;

// Sheet/page transition
static int   g_prevPage = -1;
static float g_pageAnim = 1.f;

// ============================================================
//  STYLE
// ============================================================
void UI::ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 10.f;
    s.ChildRounding      = 8.f;
    s.FrameRounding      = 6.f;
    s.PopupRounding      = 12.f;
    s.ScrollbarRounding  = 8.f;
    s.GrabRounding       = 6.f;
    s.TabRounding        = 6.f;
    s.WindowBorderSize   = 1.f;
    s.FrameBorderSize    = 0.f;
    s.PopupBorderSize    = 1.f;
    s.WindowPadding      = ImVec2(0,0);
    s.FramePadding       = ImVec2(10, 5);
    s.ItemSpacing        = ImVec2(10, 8);
    s.ItemInnerSpacing   = ImVec2(6, 4);
    s.ScrollbarSize      = 10.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.118f,0.118f,0.125f,1.f);
    c[ImGuiCol_ChildBg]         = ImVec4(0,0,0,0);
    c[ImGuiCol_PopupBg]         = ImVec4(0.14f,0.14f,0.15f,0.98f);
    c[ImGuiCol_Border]          = ImVec4(1,1,1,0.08f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.19f,0.19f,0.20f,1.f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.23f,0.23f,0.25f,1.f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.26f,0.26f,0.28f,1.f);
    c[ImGuiCol_Button]          = ImVec4(0.22f,0.22f,0.24f,1.f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.28f,0.28f,0.30f,1.f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.32f,0.32f,0.34f,1.f);
    c[ImGuiCol_Header]          = ImVec4(0.04f,0.52f,1.f,0.7f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.04f,0.52f,1.f,0.85f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.04f,0.52f,1.f,1.f);
    c[ImGuiCol_Separator]       = ImVec4(1,1,1,0.08f);
    c[ImGuiCol_Text]            = ImVec4(0.96f,0.96f,0.97f,1.f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.60f,0.60f,0.62f,1.f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.04f,0.52f,1.f,1.f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.04f,0.52f,1.f,1.f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.35f,0.70f,1.f,1.f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(1,1,1,0.20f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1,1,1,0.32f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(1,1,1,0.45f);
    c[ImGuiCol_ModalWindowDimBg]= ImVec4(0,0,0,0.45f);
}

// ============================================================
//  TEXTURES
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

// macOS card: nền xám + viền mảnh
static void MacCard(ImDrawList* dl, ImVec2 a, ImVec2 b, float r = 8.f,
                    ImU32 base = MAC_CARD_BG)
{
    dl->AddRectFilled(a, b, base, r);
    dl->AddRect(a, b, MAC_STROKE, r, 0, 1.f);
}

// ============================================================
//  ICONS  (SF Symbols vibe - stroke 2px, clean)
// ============================================================
static void I_Home(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddTriangleFilled(ImVec2(c.x, c.y-s*0.55f),
        ImVec2(c.x-s*0.62f, c.y+s*0.06f), ImVec2(c.x+s*0.62f, c.y+s*0.06f), col);
    dl->AddRectFilled(ImVec2(c.x-s*0.42f, c.y+s*0.04f),
                      ImVec2(c.x+s*0.42f, c.y+s*0.62f), col, 1.5f);
}
static void I_User(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x, c.y-s*0.18f), s*0.32f, col);
    dl->AddCircleFilled(ImVec2(c.x, c.y+s*0.55f), s*0.55f, col);
}
static void I_Grid(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float o = s*0.28f, r = s*0.22f;
    dl->AddRectFilled(ImVec2(c.x-o-r, c.y-o-r), ImVec2(c.x-o+r, c.y-o+r), col, 2.f);
    dl->AddRectFilled(ImVec2(c.x+o-r, c.y-o-r), ImVec2(c.x+o+r, c.y-o+r), col, 2.f);
    dl->AddRectFilled(ImVec2(c.x-o-r, c.y+o-r), ImVec2(c.x-o+r, c.y+o+r), col, 2.f);
    dl->AddRectFilled(ImVec2(c.x+o-r, c.y+o-r), ImVec2(c.x+o+r, c.y+o+r), col, 2.f);
}
static void I_Puzzle(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x, c.y), s*0.5f, col);
    dl->AddCircleFilled(ImVec2(c.x, c.y), s*0.22f, MAC_SIDEBAR_BG & 0x00FFFFFF);
    for (int i=0;i<6;++i){
        float a = i*IM_PI/3.f;
        dl->AddLine(ImVec2(c.x+cosf(a)*s*0.5f, c.y+sinf(a)*s*0.5f),
                    ImVec2(c.x+cosf(a)*s*0.72f, c.y+sinf(a)*s*0.72f), col, 2.2f);
    }
}
static void I_Gear(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(c, s*0.48f, col);
    dl->AddCircleFilled(c, s*0.20f, MAC_SIDEBAR_BG & 0x00FFFFFF);
    for (int i=0;i<8;++i){
        float a = i*IM_PI/4.f;
        dl->AddLine(ImVec2(c.x+cosf(a)*s*0.48f, c.y+sinf(a)*s*0.48f),
                    ImVec2(c.x+cosf(a)*s*0.70f, c.y+sinf(a)*s*0.70f), col, 2.2f);
    }
}
static void I_Chev(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.22f, c.y-s*0.35f),
                ImVec2(c.x+s*0.22f, c.y), col, 1.8f);
    dl->AddLine(ImVec2(c.x+s*0.22f, c.y),
                ImVec2(c.x-s*0.22f, c.y+s*0.35f), col, 1.8f);
}
static void I_Power(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircle(c, s*0.42f, col, 0, 2.f);
    dl->AddRectFilled(ImVec2(c.x-s*0.06f, c.y-s*0.55f),
                      ImVec2(c.x+s*0.06f, c.y+s*0.05f), col, 1.f);
}
static void I_Minus(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.35f, c.y), ImVec2(c.x+s*0.35f, c.y), col, 1.8f);
}
static void I_Max(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddTriangleFilled(ImVec2(c.x-s*0.35f, c.y+s*0.35f),
                          ImVec2(c.x+s*0.35f, c.y+s*0.35f),
                          ImVec2(c.x-s*0.35f, c.y-s*0.35f), col);
}

// ============================================================
//  TRAFFIC LIGHTS  (macOS window buttons)
// ============================================================
static void DrawTrafficLights(Launcher& L, ImDrawList* dl)
{
    const float r    = 6.f;
    const float gap  = 20.f;
    const float baseX = 16.f + r;
    const float baseY = 16.f + r;

    // hover detection cho cả nhóm
    ImVec2 mp = ImGui::GetIO().MousePos;
    bool groupHover = (mp.x >= baseX - r - 2 && mp.x <= baseX + gap*2 + r + 2 &&
                       mp.y >= baseY - r - 2 && mp.y <= baseY + r + 2);

    struct TL { ImU32 col; ImU32 colDark; const char* id; void(*icon)(ImDrawList*,ImVec2,float,ImU32); };
    TL tls[3] = {
        { TL_RED,    U32(200, 60, 55), "##tl_close", I_Minus },
        { TL_YELLOW, U32(200,150, 35), "##tl_min",   I_Minus },
        { TL_GREEN,  U32( 30,150, 50), "##tl_max",   I_Max   },
    };

    for (int i = 0; i < 3; ++i) {
        ImVec2 c(baseX + i*gap, baseY);

        ImGui::SetCursorScreenPos(ImVec2(c.x-r-4, c.y-r-4));
        ImGui::PushID(i);
        ImGui::InvisibleButton("##tl", ImVec2(r*2+8, r*2+8));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_tlHover[i] = ExpSmooth(g_tlHover[i], hov ? 1.f : 0.f, 20.f);

        // khi cả nhóm hover, hiện icon
        float iconAlpha = groupHover ? 1.f : 0.f;
        float ringAlpha = g_tlHover[i] * 0.35f;

        // outer subtle ring
        if (ringAlpha > 0.01f)
            dl->AddCircleFilled(c, r + 3.f, U32(255,255,255,(int)(60 * ringAlpha)));

        // circle
        dl->AddCircleFilled(c, r, tls[i].col);
        dl->AddCircle(c, r, U32(0,0,0, 40), 0, 0.8f);

        // icon khi hover
        if (iconAlpha > 0.01f) {
            ImU32 iconCol = U32(60, 20, 20, (int)(200 * iconAlpha));
            if (i == 1) iconCol = U32(80, 55, 10, (int)(200 * iconAlpha));
            if (i == 2) iconCol = U32(20, 60, 20, (int)(200 * iconAlpha));
            tls[i].icon(dl, c, r*0.85f, iconCol);
        }

        if (clk) {
            if (i == 0) PostQuitMessage(0);
            if (i == 1) ShowWindow(GetActiveWindow(), SW_MINIMIZE);
            if (i == 2) {
                HWND h = GetActiveWindow();
                ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE);
            }
        }
    }
}

// ============================================================
//  SIDEBAR  (macOS Finder-style)
// ============================================================
static void DrawSidebar(Launcher& L, ImDrawList* dl, ImVec2 pos, ImVec2 size)
{
    struct Item { Page page; IconFn_placeholder; };
    struct Row { Page page; void(*icon)(ImDrawList*,ImVec2,float,ImU32); const char* label; };
    static const Row rows[5] = {
        { Page::Home,     I_Home,   "Home"     },
        { Page::Accounts, I_User,   "Accounts" },
        { Page::Versions, I_Grid,   "Versions" },
        { Page::Mods,     I_Puzzle, "Mods"     },
        { Page::Settings, I_Gear,   "Settings" },
    };

    // sidebar bg
    dl->AddRectFilled(pos, ImVec2(pos.x+size.x, pos.y+size.y), MAC_SIDEBAR_BG, 0.f);
    // right border
    dl->AddLine(ImVec2(pos.x+size.x-0.5f, pos.y),
                ImVec2(pos.x+size.x-0.5f, pos.y+size.y), MAC_STROKE, 1.f);

    const float rowH  = 32.f;
    const float padX  = 10.f;
    const float rowY0 = pos.y + 52.f;   // below traffic lights

    // "Favorites" section label
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(pos.x + padX + 6, pos.y + 32.f),
                MAC_TEXT_DIM2, "LIBRARY");
    ImGui::PopFont();

    // active pill (macOS uses accent blue background for selected)
    int activeIdx = 0;
    for (int i = 0; i < 5; ++i)
        if (rows[i].page == L.s().page) activeIdx = i;

    float targetPillY = rowY0 + activeIdx * (rowH + 2.f);
    if (g_sbPillY < 0.f) g_sbPillY = targetPillY;
    g_sbPillY = ExpSmooth(g_sbPillY, targetPillY, 18.f);

    ImVec2 pillA(pos.x + 6.f, g_sbPillY);
    ImVec2 pillB(pos.x + size.x - 6.f, g_sbPillY + rowH);
    dl->AddRectFilled(pillA, pillB, MAC_ACCENT, 6.f);

    // rows
    for (int i = 0; i < 5; ++i) {
        ImVec2 rp(pos.x + 6.f, rowY0 + i*(rowH + 2.f));
        ImVec2 rs(size.x - 12.f, rowH);

        ImGui::SetCursorScreenPos(rp);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##sb", rs);
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_sbHover[i] = ExpSmooth(g_sbHover[i], hov ? 1.f : 0.f, 16.f);

        bool active = (rows[i].page == L.s().page);

        // hover bg (nếu không active)
        if (!active && g_sbHover[i] > 0.01f)
            dl->AddRectFilled(rp, ImVec2(rp.x+rs.x, rp.y+rs.y),
                              U32(255,255,255,(int)(20 * g_sbHover[i])), 6.f);

        // icon
        ImVec2 ic(rp.x + 18.f, rp.y + rowH*0.5f);
        ImU32 iconCol = active ? U32(255,255,255) : MAC_TEXT_DIM;
        rows[i].icon(dl, ic, 13.f, iconCol);

        // label
        ImGui::PushFont(g_fontRegular);
        ImU32 lblCol = active ? U32(255,255,255) : MAC_TEXT;
        dl->AddText(ImVec2(rp.x + 36.f, rp.y + (rowH - ImGui::GetTextLineHeight())*0.5f),
                    lblCol, rows[i].label);
        ImGui::PopFont();

        if (clk && !active) L.s().page = rows[i].page;
    }

    // Bottom: Discord status
    float botY = pos.y + size.y - 40.f;
    bool ok = DiscordRPC::I().isReady();
    dl->AddCircleFilled(ImVec2(pos.x + 20.f, botY + 12.f), 4.f,
                        ok ? MAC_GREEN : U32(120,120,125));
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(pos.x + 32.f, botY + 4.f),
                MAC_TEXT_DIM, ok ? "Connected" : "Offline");
    ImGui::PopFont();
}

// ============================================================
//  TOOLBAR  (title bar of macOS window)
// ============================================================
static void DrawToolbar(ImDrawList* dl, ImVec2 pos, ImVec2 size, const char* title)
{
    // toolbar bg (slightly lighter than content)
    dl->AddRectFilled(pos, ImVec2(pos.x+size.x, pos.y+size.y), MAC_TOOLBAR_BG, 0.f);
    dl->AddLine(ImVec2(pos.x, pos.y+size.y-0.5f),
                ImVec2(pos.x+size.x, pos.y+size.y-0.5f), MAC_STROKE, 1.f);

    // centered title
    ImGui::PushFont(g_fontBold);
    ImVec2 ts = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(pos.x + (size.x - ts.x)*0.5f,
                       pos.y + (size.y - ts.y)*0.5f),
                MAC_TEXT, title);
    ImGui::PopFont();
}

// ============================================================
//  iOS TOGGLE  (macOS uses the same switch)
// ============================================================
static bool MacToggle(int slot, const char* id, bool value) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = 44.f, h = 26.f;
    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clk = ImGui::IsItemClicked();
    bool hov = ImGui::IsItemHovered();

    float& a = g_toggleAnim[slot & 7];
    a = ExpSmooth(a, value ? 1.f : 0.f, 18.f);

    ImVec4 off = ImVec4(0.26f, 0.26f, 0.28f, 1.f);
    ImVec4 on  = ImGui::ColorConvertU32ToFloat4(MAC_GREEN);
    ImU32 track = ImGui::GetColorU32(ImLerp(off, on, a));
    if (hov) track = ImGui::GetColorU32(ImLerp(ImGui::ColorConvertU32ToFloat4(track),
                                               ImVec4(1,1,1,1), 0.05f));

    float r = h*0.5f;
    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), track, r);

    float kr = r - 2.f;
    float kx = p.x + r + a * (w - h);
    float ky = p.y + r;
    dl->AddCircleFilled(ImVec2(kx, ky+1.f), kr+1.f, U32(0,0,0,55));
    dl->AddCircleFilled(ImVec2(kx, ky), kr, U32(255,255,255));

    return clk;
}

// ============================================================
//  PAGE: HOME
// ============================================================
static void DrawHomePage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Hero title (macOS-large-title)
    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y + 8.f), MAC_TEXT, "RavenXD");
    ImGui::PopFont();
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(pos.x, pos.y + 56.f), MAC_TEXT_DIM,
                "Minecraft 1.8.9  ·  Forge  ·  OptiFine");
    ImGui::PopFont();

    // Launch card
    float cardW = size.x < 720.f ? size.x : 720.f;
    float cardH = 240.f;
    ImVec2 c0(pos.x + (size.x-cardW)*0.5f, pos.y + 100.f);
    ImVec2 c1(c0.x + cardW, c0.y + cardH);

    MacCard(dl, c0, c1, 12.f, MAC_CARD_BG);

    float pad = 22.f;

    // Profile row
    int ai = AccountManager::I().activeIndex();
    std::string an = AccountManager::I().activeName();
    int colorIdx = 0;
    if (ai >= 0 && ai < (int)AccountManager::I().list().size())
        colorIdx = AccountManager::I().list()[ai].colorIdx;

    int rr,gg,bb; AccountManager::avatarColor(colorIdx, rr,gg,bb);
    ImVec2 avC(c0.x + pad + 20, c0.y + pad + 20);
    dl->AddCircleFilled(avC, 20.f, U32(rr,gg,bb));
    {
        std::string ini = an.empty() ? "P" : std::string(1, toupper((unsigned char)an[0]));
        ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
        ImVec2 ts = f->CalcTextSizeA(22.f, FLT_MAX, 0, ini.c_str());
        ImGui::PushFont(f);
        dl->AddText(f, 22.f, ImVec2(avC.x-ts.x*0.5f, avC.y-ts.y*0.5f),
                    U32(255,255,255), ini.c_str());
        ImGui::PopFont();
    }
    dl->AddCircle(avC, 21.f, MAC_GREEN, 0, 1.5f);

    ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(avC.x+32, c0.y+pad+4), MAC_TEXT, an.c_str());
    ImGui::PopFont();
    dl->AddText(ImVec2(avC.x+32, c0.y+pad+26), MAC_TEXT_DIM,
                "Tap Accounts to switch profile");

    // Version combo (macOS popup button style)
    float verY = c0.y + pad + 58.f;
    const char* versions[] = { "Minecraft 1.8.9", "Forge 1.8.9", "Forge 1.8.9 + OptiFine" };
    float launchW = 140.f, launchH = 40.f;
    float comboW = c1.x - c0.x - pad*2 - launchW - 12.f;

    ImGui::SetCursorScreenPos(ImVec2(c0.x+pad, verY));
    ImGui::SetNextItemWidth(comboW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::Combo("##ver", &L.s().selectedVersion, versions, 3);
    ImGui::PopStyleVar();

    // Launch button
    bool busy = L.s().taskState == TaskState::Running;
    ImVec2 lb(c1.x - pad - launchW, verY);

    g_launchPulse += g_dt;
    float pulse = 0.5f + 0.5f*sinf(g_launchPulse*2.2f);
    int glowA = busy ? 20 : (int)(20 + 20*pulse);
    dl->AddRectFilled(ImVec2(lb.x-3,lb.y-3),
                      ImVec2(lb.x+launchW+3, lb.y+launchH+3),
                      U32(10,132,255,glowA), 9.f);

    ImGui::SetCursorScreenPos(lb);
    ImGui::PushFont(g_fontBold);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.04f,0.52f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f,0.62f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.02f,0.42f,0.86f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    if (!busy) {
        if (ImGui::Button("Launch", ImVec2(launchW, launchH))) L.onLaunchClicked();
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("...", ImVec2(launchW, launchH));
        ImGui::EndDisabled();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PopFont();

    // Progress
    float progY = verY + 56.f;
    ImGui::SetCursorScreenPos(ImVec2(c0.x+pad, progY));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.04f,0.52f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.16f,0.16f,0.18f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    static float shown = 0.f;
    shown = ExpSmooth(shown, L.s().progress, 8.f);
    char ov[80];
    if (L.s().speedMBps > 0.f)
        snprintf(ov, sizeof(ov), "%d%%   %.2f MB/s", (int)(shown*100.f), L.s().speedMBps);
    else
        snprintf(ov, sizeof(ov), "%d%%", (int)(shown*100.f));
    ImGui::ProgressBar(shown, ImVec2(c1.x - c0.x - pad*2, 8), "");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // Status text (below progress)
    float sy = progY + 20.f;
    dl->AddText(ImVec2(c0.x+pad, sy), MAC_TEXT_DIM, L.s().statusText.c_str());

    // % + speed small at right
    ImVec2 ost = ImGui::CalcTextSize(ov);
    dl->AddText(ImVec2(c1.x - pad - ost.x, progY - 20.f), MAC_TEXT_DIM, ov);

    if (L.s().taskState == TaskState::Failed && !L.s().lastError.empty()) {
        std::string err = "Error: " + L.s().lastError;
        dl->AddText(ImVec2(c0.x+pad, sy+20.f), MAC_RED, err.c_str());
    }
}

// ============================================================
//  PAGE: ACCOUNTS
// ============================================================
static void DrawAccountsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 22.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y + 8.f), MAC_TEXT, "Accounts");
    ImGui::PopFont();
    dl->AddText(ImVec2(pos.x, pos.y + 56.f), MAC_TEXT_DIM,
                "Add offline accounts and pick which one to launch with");

    // Add new (macOS group card)
    float addY = pos.y + 96.f;
    ImVec2 a0(pos.x, addY), a1(pos.x+size.x, addY+64.f);
    MacCard(dl, a0, a1, 10.f, MAC_CARD_BG);

    ImGui::SetCursorScreenPos(ImVec2(a0.x+16, a0.y+12));
    ImGui::PushFont(g_fontBold);
    ImGui::Text("New account");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(a0.x+16, a0.y+34));
    ImGui::SetNextItemWidth(260);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
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

    // List
    float listY = addY + 76.f;
    ImGui::SetCursorScreenPos(ImVec2(pos.x, listY));
    ImGui::BeginChild("##acclist", ImVec2(size.x, size.y-(listY-pos.y)-pad), false);

    auto& list = AccountManager::I().list();
    for (size_t i=0;i<list.size();++i) {
        auto& acc = list[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 72.f;

        ImU32 bg = acc.active ? U32(24, 60, 108) : MAC_CARD_BG;
        MacCard(dl, cp, ImVec2(cp.x+cw, cp.y+ch), 10.f, bg);

        // Avatar
        int rr,gg,bb; AccountManager::avatarColor(acc.colorIdx, rr,gg,bb);
        ImVec2 av(cp.x+38, cp.y+ch*0.5f);
        dl->AddCircleFilled(av, 20.f, U32(rr,gg,bb));
        std::string ini = acc.name.empty() ? "P" : std::string(1, toupper((unsigned char)acc.name[0]));
        ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
        ImVec2 ts = f->CalcTextSizeA(22.f, FLT_MAX, 0, ini.c_str());
        ImGui::PushFont(f);
        dl->AddText(f, 22.f, ImVec2(av.x-ts.x*0.5f, av.y-ts.y*0.5f),
                    U32(255,255,255), ini.c_str());
        ImGui::PopFont();
        if (acc.active) dl->AddCircle(av, 21.f, MAC_GREEN, 0, 1.5f);

        // Name + status
        ImGui::SetCursorScreenPos(ImVec2(cp.x+72, cp.y+12));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", acc.name.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+72, cp.y+34));
        ImGui::PushStyleColor(ImGuiCol_Text,
            acc.active ? ImVec4(0.19f,0.82f,0.35f,1.f) : ImVec4(0.60f,0.60f,0.62f,1.f));
        ImGui::Text("%s", acc.active ? "Active" : "Offline");
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+72, cp.y+52));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f,0.40f,0.42f,1.f));
        std::string uu = acc.uuid.substr(0, 20) + "...";
        ImGui::Text("%s", uu.c_str());
        ImGui::PopStyleColor();

        // Buttons
        float bx = cp.x + cw - 340.f;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + ch*0.5f - 14.f));
        if (!acc.active) {
            if (ImGui::Button("Set Active", ImVec2(100, 28)))
                AccountManager::I().setActive(i);
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("Current", ImVec2(100, 28));
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Rename", ImVec2(90, 28))) {
            std::string nn = acc.name + "_2";
            AccountManager::I().rename(i, nn);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(90, 28))) {
            AccountManager::I().remove(i);
            break;
        }

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + ch + 8));
    }
    ImGui::EndChild();
}

// ============================================================
//  PAGE: VERSIONS
// ============================================================
struct VerItem { const char* title; const char* sub; const char* tag;
                 bool free; int mapSel; ImU32 c1, c2, accent; };

static VerItem g_versions[4] = {
    { "1.16.5",        "Nether",  "Client", false, 0, U32(180,45,90),  U32(255,90,60),   U32(255,130,90) },
    { "ALPHA 1.16.5",  "Aurora",  "Client", false, 1, U32(20,130,130), U32(60,230,190),  U32(80,240,200) },
    { "LEGACY 1.12.2", "Classic", "Free",   true,  2, U32(60,140,230), U32(140,200,255), U32(120,180,255)},
    { "1.21.11",       "Forest",  "Client", false, 1, U32(50,150,70),  U32(150,220,110), U32(130,210,120)},
};

static void DrawVersionsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 22.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y + 8.f), MAC_TEXT, "Versions");
    ImGui::PopFont();
    dl->AddText(ImVec2(pos.x, pos.y + 56.f), MAC_TEXT_DIM,
                "Pick a client to install & launch");

    float gridY = pos.y + 100.f;
    float gap = 14.f;
    int cols = 3;
    float cardW = (size.x - gap*(cols-1)) / cols;
    float cardH = 160.f;

    for (int i=0;i<4;++i) {
        int row = i/cols, col = i%cols;
        ImVec2 cp(pos.x + col*(cardW+gap), gridY + row*(cardH+gap));
        if (cp.x + cardW > pos.x + size.x) continue;

        ImGui::SetCursorScreenPos(cp);
        ImGui::InvisibleButton(g_versions[i].title, ImVec2(cardW, cardH));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        g_verHover[i] = ExpSmooth(g_verHover[i], hov?1.f:0.f, 12.f);

        float grow = g_verHover[i]*3.f;
        ImVec2 g0(cp.x-grow*0.5f, cp.y-grow*0.5f);
        ImVec2 g1(cp.x+cardW+grow*0.5f, cp.y+cardH+grow*0.5f);

        GradV(dl, g0, g1, g_versions[i].c1, g_versions[i].c2, 10.f);
        dl->AddRectFilled(g0, g1, U32(0,0,0,(int)(80 - 30*g_verHover[i])), 10.f);

        bool sel = (L.s().selectedVersion == g_versions[i].mapSel);
        if (sel)
            dl->AddRect(g0, g1, g_versions[i].accent, 10.f, 0, 2.5f);
        else if (g_verHover[i] > 0.01f)
            dl->AddRect(g0, g1, U32(255,255,255,(int)(180*g_verHover[i])), 10.f, 0, 1.5f);

        // Tag
        const char* tg = g_versions[i].tag;
        ImVec2 tsz = ImGui::CalcTextSize(tg);
        ImVec2 tp(g1.x - tsz.x - 24, g0.y + 10);
        dl->AddRectFilled(tp, ImVec2(tp.x+tsz.x+16, tp.y+tsz.y+6),
                          g_versions[i].free ? MAC_GREEN : U32(0,0,0,160), 4.f);
        dl->AddText(ImVec2(tp.x+8, tp.y+3), U32(255,255,255), tg);

        // Title
        ImGui::PushFont(g_fontBold);
        ImVec2 tt = ImGui::CalcTextSize(g_versions[i].title);
        ImVec2 tpos(g0.x+14, g1.y - tt.y - 32.f);
        dl->AddText(tpos, U32(255,255,255), g_versions[i].title);
        ImGui::PopFont();

        ImVec2 ss = ImGui::CalcTextSize(g_versions[i].sub);
        dl->AddText(ImVec2(g0.x+14, g1.y - ss.y - 12.f),
                    U32(230,235,245,210), g_versions[i].sub);

        I_Chev(dl, ImVec2(g1.x-20, g1.y-20), 11.f, U32(255,255,255,220));

        if (clk) {
            L.s().selectedVersion = g_versions[i].mapSel;
            Settings::I().d().version = g_versions[i].title;
            Settings::I().save();
            L.onLaunchClicked();
        }
    }
}

// ============================================================
//  PAGE: MODS
// ============================================================
static void DrawModsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 22.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y + 8.f), MAC_TEXT, "Mods");
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 130, pos.y + 12));
    if (ImGui::Button("Update All", ImVec2(130, 30)))
        L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 66.f));
    ImGui::BeginChild("##modscroll",
        ImVec2(size.x, size.y - 66.f - pad), false);

    auto& mods = L.mods().mods();
    for (size_t i=0;i<mods.size();++i) {
        auto& m = mods[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 76.f;

        MacCard(dl, cp, ImVec2(cp.x+cw, cp.y+ch), 10.f, MAC_CARD_BG);

        ImGui::SetCursorScreenPos(ImVec2(cp.x+16, cp.y+12));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", m.name.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+16, cp.y+36));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f,0.60f,0.62f,1.f));
        ImGui::Text("v%s  ·  %s  ·  %s", m.version.c_str(), m.source.c_str(),
                    m.installed ? "installed" : "not installed");
        ImGui::PopStyleColor();

        float bx = cp.x + cw - 320.f;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + ch*0.5f - 14.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            m.enabled ? ImVec4(0.19f,0.82f,0.35f,1.f) : ImVec4(0.75f,0.35f,0.35f,1.f));
        ImGui::Text("%s", m.enabled ? "ON" : "OFF");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button(m.enabled?"Disable":"Enable", ImVec2(80,28)))
            L.mods().setEnabled(i, !m.enabled, Settings::I().d().minecraftDir);
        ImGui::SameLine();
        if (ImGui::Button("Update", ImVec2(80,28)))
            L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(80,28)))
            L.mods().remove(i, Settings::I().d().minecraftDir);

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + ch + 8.f));
    }
    ImGui::EndChild();
}

// ============================================================
//  PAGE: SETTINGS
// ============================================================
static void DrawSettingsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    (void)L;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& d = Settings::I().d();
    float pad = 22.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y + 8.f), MAC_TEXT, "Settings");
    ImGui::PopFont();

    float cy = pos.y + 64.f;
    float cw = size.x;

    auto Card = [&](float h) {
        ImVec2 a(pos.x, cy), b(pos.x+cw, cy+h);
        MacCard(dl, a, b, 10.f, MAC_CARD_BG);
        ImGui::SetCursorScreenPos(ImVec2(a.x+16, a.y+12));
    };

    // --- Minecraft dir ---
    Card(72);
    ImGui::Text("Minecraft Directory");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+38));
    static char dirB[512];
    strncpy_s(dirB, d.minecraftDir.c_str(), sizeof(dirB)-1);
    ImGui::SetNextItemWidth(cw - 32);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    if (ImGui::InputText("##dir", dirB, sizeof(dirB))) {
        d.minecraftDir = dirB; Settings::I().save();
    }
    ImGui::PopStyleVar();
    cy += 82.f;

    // --- Java path ---
    Card(72);
    ImGui::Text("Java Path");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+38));
    static char jB[512];
    strncpy_s(jB, d.javaPath.c_str(), sizeof(jB)-1);
    ImGui::SetNextItemWidth(cw - 180);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    if (ImGui::InputText("##java", jB, sizeof(jB))) {
        d.javaPath = jB; Settings::I().save();
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (ImGui::Button("Auto Detect", ImVec2(130, 0))) {
        d.javaPath = Minecraft::FindJava();
        Settings::I().save();
        strncpy_s(jB, d.javaPath.c_str(), sizeof(jB)-1);
    }
    cy += 82.f;

    // --- RAM + window ---
    Card(120);
    ImGui::Text("RAM (MB)");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+38));
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderInt("##ram", &d.ramMB, 512, 16384)) Settings::I().save();

    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+72));
    ImGui::Text("Window");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+90, cy+72));
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputInt("##ww", &d.windowWidth, 0, 0)) Settings::I().save();
    ImGui::SameLine(); ImGui::Text("×"); ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputInt("##wh", &d.windowHeight, 0, 0)) Settings::I().save();
    cy += 130.f;

    // --- Toggles ---
    Card(110);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Close launcher after launch");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+cw-16-44, cy+10));
    if (MacToggle(0, "##t_close", d.closeAfterLaunch)) {
        d.closeAfterLaunch = !d.closeAfterLaunch;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+44));
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Debug Mode");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+cw-16-44, cy+40));
    if (MacToggle(1, "##t_debug", d.debugMode)) {
        d.debugMode = !d.debugMode;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+76));
    if (ImGui::Button("Open RavenXD Folder", ImVec2(200, 26))) {
        std::string p = Settings::I().appDataDir();
        ShellExecuteA(nullptr, "open", p.c_str(), nullptr, nullptr, SW_SHOW);
    }
    cy += 120.f;

    // --- Discord ---
    bool dc = DiscordRPC::I().isReady();
    ImGui::SetCursorScreenPos(ImVec2(pos.x, cy));
    ImGui::TextColored(dc ? ImVec4(0.19f,0.82f,0.35f,1.f)
                          : ImVec4(0.60f,0.60f,0.62f,1.f),
                       dc ? "Discord: Connected" : "Discord: Not running");
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
        ImGui::TextColored(ImVec4(1.f,0.4f,0.4f,1.f), "Java not found");
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
            o.lpstrFile = f; o.nMaxFile = MAX_PATH;
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
    dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, MAC_WINDOW_BG);

    float t = L.s().loadingTimer;
    float a = t < 0.6f ? t/0.6f : 1.f;
    float sc = t > 1.f ? 1.f : 0.85f + 0.15f*t;
    float cx = io.DisplaySize.x*0.5f, cy = io.DisplaySize.y*0.5f - 30.f;
    float sz = 120.f * sc;

    ensureLogo();
    if (g_logoTex) {
        dl->AddImage((ImTextureID)g_logoTex,
            ImVec2(cx-sz*0.5f, cy-sz*0.5f), ImVec2(cx+sz*0.5f, cy+sz*0.5f),
            ImVec2(0,0), ImVec2(1,1), U32(255,255,255,(int)(a*255)));
    } else {
        ImGui::PushFont(g_fontBold);
        ImGui::SetCursorScreenPos(ImVec2(cx-50, cy-16));
        ImGui::TextColored(ImVec4(1,1,1,a), "RavenXD");
        ImGui::PopFont();
    }

    const char* ph = t < 0.8f ? "Checking files..." :
                     t < 1.6f ? "Loading launcher..." : "Ready";
    ImVec2 ts = ImGui::CalcTextSize(ph);
    dl->AddText(ImVec2(cx-ts.x*0.5f, cy+90.f), U32(150,150,155,(int)(a*255)), ph);
}

// ============================================================
//  ROOT
// ============================================================
void UI::Render(Launcher& L, float dt)
{
    g_dt = dt > 0.f ? dt : (1.f/60.f);
    L.tick(dt);

    if (L.s().showLoadingScreen) { RenderLoadingScreen(L); return; }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // ---------- Window background (macOS window chrome) ----------
    dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, MAC_WINDOW_BG);

    // subtle outer stroke
    dl->AddRect(ImVec2(0.5f, 0.5f), ImVec2(io.DisplaySize.x-0.5f, io.DisplaySize.y-0.5f),
                MAC_STROKE_HI, 10.f, 0, 1.f);

    // ---------- Layout constants ----------
    const float sidebarW = 200.f;
    const float toolbarH = 44.f;
    const float contentX = sidebarW;
    const float contentY = toolbarH;
    const float contentW = io.DisplaySize.x - sidebarW;
    const float contentH = io.DisplaySize.y - toolbarH;

    // Sidebar
    DrawSidebar(L, dl, ImVec2(0, 0), ImVec2(sidebarW, io.DisplaySize.y));

    // Toolbar (in content area only — sidebar has its own title area)
    const char* title = "RavenXD";
    switch (L.s().page) {
        case Page::Home:     title = "RavenXD";    break;
        case Page::Accounts: title = "Accounts";   break;
        case Page::Versions: title = "Versions";   break;
        case Page::Mods:     title = "Mods";       break;
        case Page::Settings: title = "Settings";   break;
    }
    DrawToolbar(dl, ImVec2(contentX, 0), ImVec2(contentW, toolbarH), title);

    // ---------- Content area (single window) ----------
    ImGui::SetNextWindowPos(ImVec2(contentX, contentY));
    ImGui::SetNextWindowSize(ImVec2(contentW, contentH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##content", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    // page transition animation
    const Page cur = L.s().page;
    int curIdx = 0;
    switch (cur) {
        case Page::Home: curIdx=0; break;
        case Page::Accounts: curIdx=1; break;
        case Page::Versions: curIdx=2; break;
        case Page::Mods: curIdx=3; break;
        case Page::Settings: curIdx=4; break;
    }
    if (g_prevPage != curIdx) { g_prevPage = curIdx; g_pageAnim = 0.f; }
    g_pageAnim = ExpSmooth(g_pageAnim, 1.f, 14.f);
    float ease = EaseOutCubic(g_pageAnim);

    ImVec2 pagePos(28.f, 24.f + (1.f - ease) * 8.f);
    ImVec2 pageSize(contentW - 56.f, contentH - 48.f);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ease);

    switch (cur) {
        case Page::Home:     DrawHomePage    (L, pagePos, pageSize); break;
        case Page::Accounts: DrawAccountsPage(L, pagePos, pageSize); break;
        case Page::Versions: DrawVersionsPage(L, pagePos, pageSize); break;
        case Page::Mods:     DrawModsPage    (L, pagePos, pageSize); break;
        case Page::Settings: DrawSettingsPage(L, pagePos, pageSize); break;
    }
    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleVar();

    // ---------- Traffic lights (drawn LAST so they're always on top) ----------
    // These sit at the top-left of the window, on top of the sidebar.
    DrawTrafficLights(L, dl);

    DrawJavaPopup(L);
}
