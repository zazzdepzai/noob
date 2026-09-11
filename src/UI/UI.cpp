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

// ============================================================
//  BLUE THEME
// ============================================================
static inline ImU32 U32(int r,int g,int b,int a=255){ return IM_COL32(r,g,b,a); }

static const ImU32 C_BG         = U32(  7,  9, 13);
static const ImU32 C_SIDEBAR    = U32( 13, 16, 23, 235);
static const ImU32 C_CARD       = U32( 19, 23, 32, 235);
static const ImU32 C_CARD_HOV   = U32( 28, 35, 49, 245);
static const ImU32 C_STROKE     = U32(255,255,255, 26);
static const ImU32 C_TEXT       = U32(248,250,252);
static const ImU32 C_TEXT_DIM   = U32(156,163,175);
static const ImU32 C_TEXT_DIM2  = U32(107,114,128);

static const ImU32 C_ACCENT     = U32( 73,145,255);
static const ImU32 C_ACCENT_HI  = U32(125,180,255);
static const ImU32 C_ACCENT_LO  = U32( 31, 96,214);
static const ImU32 C_GREEN      = U32( 34,197, 94);
static const ImU32 C_RED        = U32(239, 68, 68);
static const ImU32 C_YELLOW     = U32(250,204, 21);

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
static inline float EaseOutCubic(float t){ return 1.f - powf(1.f-t, 3.f); }

static float g_sideHover[5]  = {0};
static float g_sidePillY     = -1.f;
static float g_launchPulse   = 0.f;
static int   g_prevPage      = -1;
static float g_pageAnim      = 1.f;

// ============================================================
//  STYLE
// ============================================================
void UI::ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 24.f;
    s.ChildRounding      = 18.f;
    s.FrameRounding      = 14.f;
    s.PopupRounding      = 18.f;
    s.ScrollbarRounding  = 10.f;
    s.GrabRounding       = 10.f;
    s.WindowBorderSize   = 0.f;
    s.FrameBorderSize    = 0.f;
    s.PopupBorderSize    = 1.f;
    s.WindowPadding      = ImVec2(0,0);
    s.FramePadding       = ImVec2(12, 6);
    s.ItemSpacing        = ImVec2(10, 8);
    s.ItemInnerSpacing   = ImVec2(6, 4);
    s.ScrollbarSize      = 8.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.025f,0.030f,0.045f,1.f);
    c[ImGuiCol_ChildBg]         = ImVec4(0,0,0,0);
    c[ImGuiCol_PopupBg]         = ImVec4(0.050f,0.065f,0.095f,0.985f);
    c[ImGuiCol_Border]          = ImVec4(1,1,1,0.07f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.070f,0.085f,0.115f,1.f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.095f,0.120f,0.165f,1.f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.120f,0.155f,0.215f,1.f);
    c[ImGuiCol_Button]          = ImVec4(0.070f,0.090f,0.125f,1.f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.105f,0.140f,0.195f,1.f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.145f,0.205f,0.300f,1.f);
    c[ImGuiCol_Header]           = ImVec4(0.285f,0.570f,0.980f,0.65f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.380f,0.660f,1.000f,0.78f);
    c[ImGuiCol_HeaderActive]     = ImVec4(0.380f,0.660f,1.000f,0.95f);
    c[ImGuiCol_Separator]        = ImVec4(1,1,1,0.075f);
    c[ImGuiCol_Text]             = ImVec4(0.97f,0.98f,1.00f,1.f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.55f,0.59f,0.67f,1.f);
    c[ImGuiCol_CheckMark]        = ImVec4(0.29f,0.60f,1.00f,1.f);
    c[ImGuiCol_SliderGrab]       = ImVec4(0.29f,0.60f,1.00f,1.f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.48f,0.75f,1.00f,1.f);
    c[ImGuiCol_ScrollbarBg]      = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]    = ImVec4(1,1,1,0.16f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1,1,1,0.28f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0,0,0,0.62f);
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

static void Card(ImDrawList* dl, ImVec2 a, ImVec2 b, float r = 18.f,
                 ImU32 base = C_CARD)
{
    dl->AddRectFilled(ImVec2(a.x, a.y + 7.f), ImVec2(b.x, b.y + 7.f), U32(0,0,0,34), r + 2.f);
    dl->AddRectFilled(a, b, base, r);
    dl->AddRect(a, b, C_STROKE, r, 0, 1.f);
}

static void GlowRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float r, float strength = 1.f)
{
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    for (int i = 4; i >= 1; --i) {
        float ex = i * 5.f;
        int alpha = (int)(8.f * strength * (5 - i));
        ImU32 glow = U32((int)(c.x * 255.f), (int)(c.y * 255.f),
                         (int)(c.z * 255.f), alpha);
        dl->AddRectFilled(ImVec2(a.x-ex, a.y-ex), ImVec2(b.x+ex, b.y+ex), glow, r+ex);
    }
}

// ============================================================
//  ICONS (SF symbols style)
// ============================================================
static void I_Home(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddTriangleFilled(ImVec2(c.x, c.y-s*0.55f),
        ImVec2(c.x-s*0.60f, c.y+s*0.05f), ImVec2(c.x+s*0.60f, c.y+s*0.05f), col);
    dl->AddRectFilled(ImVec2(c.x-s*0.40f, c.y+s*0.05f),
        ImVec2(c.x+s*0.40f, c.y+s*0.60f), col, 1.5f);
}
static void I_User(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x, c.y-s*0.18f), s*0.30f, col);
    dl->AddCircleFilled(ImVec2(c.x, c.y+s*0.52f), s*0.50f, col);
}
static void I_Grid(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float o = s*0.28f, r = s*0.22f;
    dl->AddRectFilled(ImVec2(c.x-o-r,c.y-o-r), ImVec2(c.x-o+r,c.y-o+r), col, 2.f);
    dl->AddRectFilled(ImVec2(c.x+o-r,c.y-o-r), ImVec2(c.x+o+r,c.y-o+r), col, 2.f);
    dl->AddRectFilled(ImVec2(c.x-o-r,c.y+o-r), ImVec2(c.x-o+r,c.y+o+r), col, 2.f);
    dl->AddRectFilled(ImVec2(c.x+o-r,c.y+o-r), ImVec2(c.x+o+r,c.y+o+r), col, 2.f);
}
static void I_Puzzle(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(c, s*0.45f, col);
    dl->AddCircleFilled(c, s*0.20f, C_SIDEBAR);
    for (int i=0;i<6;++i){
        float a = i*IM_PI/3.f;
        dl->AddLine(ImVec2(c.x+cosf(a)*s*0.45f, c.y+sinf(a)*s*0.45f),
                    ImVec2(c.x+cosf(a)*s*0.68f, c.y+sinf(a)*s*0.68f), col, 2.2f);
    }
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
static void I_Minus(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.35f, c.y), ImVec2(c.x+s*0.35f, c.y), col, 2.f);
}
static void I_Close(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.28f,c.y-s*0.28f),
                ImVec2(c.x+s*0.28f,c.y+s*0.28f), col, 2.f);
    dl->AddLine(ImVec2(c.x+s*0.28f,c.y-s*0.28f),
                ImVec2(c.x-s*0.28f,c.y+s*0.28f), col, 2.f);
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
    const float barH = 64.f;
    dl->AddRectFilled(ImVec2(0,0), ImVec2(disp.x, barH), U32(10,13,19,245), 0.f);
    dl->AddLine(ImVec2(18, barH-0.5f), ImVec2(disp.x-18.f, barH-0.5f), C_STROKE, 1.f);

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(26, barH*0.5f - 15.f), C_TEXT, "RavenXD");
    dl->AddCircleFilled(ImVec2(17.f, barH*0.5f), 3.f, C_ACCENT_HI);
    ImGui::PopFont();

    // Discord status
    bool dc = DiscordRPC::I().isReady();
    float closeX = disp.x - 22.f - 16.f;
    float closeY = barH*0.5f;

    // Close
    ImGui::SetCursorScreenPos(ImVec2(closeX-16, closeY-16));
    ImGui::InvisibleButton("##close", ImVec2(32,32));
    bool closeHov = ImGui::IsItemHovered();
    bool closeClk = ImGui::IsItemClicked();
    if (closeHov) dl->AddCircleFilled(ImVec2(closeX, closeY), 14.f, U32(220,50,50,150));
    I_Close(dl, ImVec2(closeX, closeY), 10.f, C_TEXT);

    // Minimize
    float minX = closeX - 32.f - 8.f;
    ImGui::SetCursorScreenPos(ImVec2(minX-16, closeY-16));
    ImGui::InvisibleButton("##min", ImVec2(32,32));
    bool minHov = ImGui::IsItemHovered();
    bool minClk = ImGui::IsItemClicked();
    if (minHov) dl->AddCircleFilled(ImVec2(minX, closeY), 14.f, U32(255,255,255,25));
    I_Minus(dl, ImVec2(minX, closeY), 10.f, C_TEXT);

    // Discord indicator
    ImVec2 dcC(minX - 32.f - 8.f, closeY);
    dl->AddCircleFilled(dcC, 6.f, dc ? C_GREEN : U32(120,120,130));
    dl->AddText(ImVec2(dcC.x - 90.f, closeY - 8.f), C_TEXT_DIM,
                dc ? "Discord: Connected" : "Discord: Offline");

    if (closeClk) PostQuitMessage(0);
    if (minClk)   ShowWindow(GetActiveWindow(), SW_MINIMIZE);
}

// ============================================================
//  SIDEBAR
// ============================================================
static void DrawSidebar(Launcher& L, ImDrawList* dl, ImVec2 pos, ImVec2 size)
{
    struct Row { Page page; void(*icon)(ImDrawList*,ImVec2,float,ImU32); const char* label; };
    static const Row rows[4] = {
        { Page::Home,     I_Home,   "Home"     },
        { Page::Accounts, I_User,   "Accounts" },
        { Page::Mods,     I_Puzzle, "Mods"     },
        { Page::Settings, I_Gear,   "Settings" },
    };

    Card(dl, ImVec2(pos.x+12.f, pos.y+12.f), ImVec2(pos.x+size.x-12.f, pos.y+size.y-12.f), 22.f, C_SIDEBAR);
    dl->AddLine(ImVec2(pos.x+size.x-0.5f, pos.y),
                ImVec2(pos.x+size.x-0.5f, pos.y+size.y), C_STROKE, 1.f);

    const float rowH = 48.f, gapY = 7.f, startY = pos.y + 38.f;
    const float padX = 22.f;
    float rowW = size.x - padX*2;

    int cur = (int)L.s().page;
    int activeIdx = 0;
    for (int i = 0; i < 4; ++i)
        if ((int)rows[i].page == cur) activeIdx = i;

    float targetPillY = startY + activeIdx*(rowH + gapY);
    if (g_sidePillY < 0.f) g_sidePillY = targetPillY;
    g_sidePillY = ExpSmooth(g_sidePillY, targetPillY, 18.f);

    // Active pill
    ImVec2 pillA(pos.x + padX, g_sidePillY);
    ImVec2 pillB(pillA.x + rowW, g_sidePillY + rowH);
    GradV(dl, pillA, pillB, C_ACCENT_HI, C_ACCENT_LO, 16.f);

    for (int i = 0; i < 4; ++i) {
        ImVec2 rp(pos.x + padX, startY + i*(rowH+gapY));
        ImVec2 rs(rowW, rowH);

        ImGui::SetCursorScreenPos(rp);
        ImGui::PushID(1000 + i);
        ImGui::InvisibleButton("##side", rs);
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_sideHover[i] = ExpSmooth(g_sideHover[i], hov ? 1.f : 0.f, 16.f);
        bool active = ((int)rows[i].page == cur);

        if (!active && g_sideHover[i] > 0.01f)
            dl->AddRectFilled(rp, ImVec2(rp.x+rs.x, rp.y+rs.y),
                              U32(255,255,255,(int)(20*g_sideHover[i])), 14.f);

        ImVec2 ic(rp.x + 20, rp.y + rowH*0.5f);
        ImU32 iconCol = active ? U32(255,255,255) : C_TEXT_DIM;
        rows[i].icon(dl, ic, 13.f, iconCol);

        dl->AddText(ImVec2(rp.x + 42, rp.y + (rowH - ImGui::GetTextLineHeight())*0.5f),
                    active ? U32(255,255,255) : C_TEXT, rows[i].label);

        if (clk && !active) {
            L.s().page = rows[i].page;
            FX::PlayFXSound(FX::SND_CLICK);
        }
    }
}

// ============================================================
//  HOME PAGE
// ============================================================
static void DrawHomePage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Hero title
    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y + 8.f), C_TEXT, "Home");
    ImGui::PopFont();
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(pos.x, pos.y + 56.f), C_TEXT_DIM,
                "Launch Minecraft 1.8.9 with Forge + OptiFine");
    ImGui::PopFont();

    // Launch card
    float cardW = size.x < 720.f ? size.x : 720.f;
    float cardH = 260.f;
    ImVec2 c0(pos.x + (size.x-cardW)*0.5f, pos.y + 100.f);
    ImVec2 c1(c0.x + cardW, c0.y + cardH);

    Card(dl, c0, c1, 22.f, C_CARD);

    float pad = 24.f;

    // Profile row
    int ai = AccountManager::I().activeIndex();
    std::string an = AccountManager::I().activeName();
    int colorIdx = 0;
    if (ai >= 0 && ai < (int)AccountManager::I().list().size())
        colorIdx = AccountManager::I().list()[ai].colorIdx;

    int rr,gg,bb; AccountManager::avatarColor(colorIdx, rr,gg,bb);
    ImVec2 avC(c0.x + pad + 24, c0.y + pad + 24);
    dl->AddCircleFilled(avC, 24.f, U32(rr,gg,bb));
    {
        std::string ini = an.empty() ? "P" : std::string(1, toupper((unsigned char)an[0]));
        ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
        ImVec2 ts = f->CalcTextSizeA(26.f, FLT_MAX, 0, ini.c_str());
        ImGui::PushFont(f);
        dl->AddText(f, 26.f, ImVec2(avC.x-ts.x*0.5f, avC.y-ts.y*0.5f),
                    U32(255,255,255), ini.c_str());
        ImGui::PopFont();
    }
    dl->AddCircle(avC, 25.f, C_GREEN, 0, 2.f);

    ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(avC.x + 38, c0.y + pad + 8), C_TEXT, an.c_str());
    ImGui::PopFont();
    dl->AddText(ImVec2(avC.x + 38, c0.y + pad + 32), C_TEXT_DIM,
                "Tap Accounts to switch profile");

    // Version combo + Launch
    float verY = c0.y + pad + 72.f;
    const char* versions[] = { "Minecraft 1.8.9", "Forge 1.8.9", "Forge 1.8.9 + OptiFine" };
    float launchW = 160.f, launchH = 48.f;
    float comboW = c1.x - c0.x - pad*2 - launchW - 12.f;

    ImGui::SetCursorScreenPos(ImVec2(c0.x+pad, verY));
    ImGui::SetNextItemWidth(comboW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    ImGui::Combo("##ver", &L.s().selectedVersion, versions, 3);
    ImGui::PopStyleVar();

    // Launch button
    bool busy = L.s().taskState == TaskState::Running;
    ImVec2 lb(c1.x - pad - launchW, verY);

    g_launchPulse += g_dt;
    float pulse = 0.5f + 0.5f*sinf(g_launchPulse*2.2f);
    int glowA = busy ? 20 : (int)(20 + 20*pulse);
    GlowRect(dl, ImVec2(lb.x, lb.y), ImVec2(lb.x+launchW, lb.y+launchH), C_ACCENT, 18.f, 1.0f + pulse);
    dl->AddRectFilled(ImVec2(lb.x-2,lb.y-2),
                      ImVec2(lb.x+launchW+2, lb.y+launchH+2),
                      U32(59,130,246,glowA), 18.f);

    ImGui::SetCursorScreenPos(lb);
    ImGui::PushFont(g_fontBold);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.04f,0.52f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f,0.62f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.02f,0.42f,0.86f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (!busy) {
        if (ImGui::Button("LAUNCH", ImVec2(launchW, launchH))) {
            L.onLaunchClicked();
            FX::PlayFXSound(FX::SND_SUCCESS);
            FX::TriggerCursorPulse();
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("...", ImVec2(launchW, launchH));
        ImGui::EndDisabled();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PopFont();

    // Progress
    float progY = verY + 62.f;
    ImGui::SetCursorScreenPos(ImVec2(c0.x+pad, progY));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.04f,0.52f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.16f,0.16f,0.18f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    static float shown = 0.f;
    shown = ExpSmooth(shown, L.s().progress, 8.f);
    char ov[80];
    if (L.s().speedMBps > 0.f)
        snprintf(ov, sizeof(ov), "%d%%  %.2f MB/s", (int)(shown*100.f), L.s().speedMBps);
    else
        snprintf(ov, sizeof(ov), "%d%%", (int)(shown*100.f));
    ImGui::ProgressBar(shown, ImVec2(c1.x - c0.x - pad*2, 10), "");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // Status text
    float sy = progY + 24.f;
    dl->AddText(ImVec2(c0.x+pad, sy), C_TEXT_DIM, L.s().statusText.c_str());

    // % + speed
    ImVec2 ost = ImGui::CalcTextSize(ov);
    dl->AddText(ImVec2(c1.x - pad - ost.x, progY - 20.f), C_TEXT_DIM, ov);

    if (L.s().taskState == TaskState::Failed && !L.s().lastError.empty()) {
        std::string err = "Error: " + L.s().lastError;
        dl->AddText(ImVec2(c0.x+pad, sy+22.f), C_RED, err.c_str());
    }
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

    // Add new
    float addY = pos.y + 88.f;
    ImVec2 a0(pos.x, addY), a1(pos.x+size.x, addY+64.f);
    Card(dl, a0, a1, 18.f, C_CARD);

    ImGui::SetCursorScreenPos(ImVec2(a0.x+16, a0.y+14));
    ImGui::PushFont(g_fontBold);
    ImGui::Text("New account");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(a0.x+16, a0.y+36));
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
            FX::PlayFXSound(FX::SND_CHECK);
        }
    }

    // List
    float listY = addY + 78.f;
    ImGui::SetCursorScreenPos(ImVec2(pos.x, listY));
    ImGui::BeginChild("##acclist", ImVec2(size.x, size.y-(listY-pos.y)-pad), false);

    auto& list = AccountManager::I().list();
    for (size_t i=0;i<list.size();++i) {
        auto& acc = list[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 72.f;

        ImU32 bg = acc.active ? U32(29, 78,160) : C_CARD;
        Card(dl, cp, ImVec2(cp.x+cw, cp.y+ch), 18.f, bg);

        int rr,gg,bb; AccountManager::avatarColor(acc.colorIdx, rr,gg,bb);
        ImVec2 av(cp.x+38, cp.y+ch*0.5f);
        dl->AddCircleFilled(av, 20.f, U32(rr,gg,bb));
        std::string ini = acc.name.empty() ? "P"
                        : std::string(1, toupper((unsigned char)acc.name[0]));
        ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
        ImVec2 ts = f->CalcTextSizeA(22.f, FLT_MAX, 0, ini.c_str());
        ImGui::PushFont(f);
        dl->AddText(f, 22.f, ImVec2(av.x-ts.x*0.5f, av.y-ts.y*0.5f),
                    U32(255,255,255), ini.c_str());
        ImGui::PopFont();
        if (acc.active) dl->AddCircle(av, 21.f, C_GREEN, 0, 2.f);

        ImGui::SetCursorScreenPos(ImVec2(cp.x+72, cp.y+12));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", acc.name.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+72, cp.y+34));
        ImGui::PushStyleColor(ImGuiCol_Text,
            acc.active ? ImVec4(0.13f,0.77f,0.37f,1.f) : ImVec4(0.60f,0.64f,0.72f,1.f));
        ImGui::Text("%s", acc.active ? "Active" : "Offline");
        ImGui::PopStyleColor();

        // Buttons
        float bx = cp.x + cw - 340.f;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + ch*0.5f - 14.f));
        if (!acc.active) {
            if (ImGui::Button("Set Active", ImVec2(100, 28))) {
                AccountManager::I().setActive(i);
                FX::PlayFXSound(FX::SND_CHECK);
            }
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
            FX::PlayFXSound(FX::SND_UNCHECK);
            break;
        }

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + ch + 8));
    }
    ImGui::EndChild();
}

// ============================================================
//  PAGE: MODS
// ============================================================
static void DrawModsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 24.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y), C_TEXT, "Mods");
    ImGui::PopFont();
    dl->AddText(ImVec2(pos.x, pos.y + 48.f), C_TEXT_DIM,
                "Manage installed mods — enable, disable, update");

    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 130, pos.y + 8));
    if (ImGui::Button("Update All", ImVec2(130, 30)))
        L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 88.f));
    ImGui::BeginChild("##modscroll",
        ImVec2(size.x, size.y - 88.f - pad), false);

    auto& mods = L.mods().mods();
    for (size_t i=0;i<mods.size();++i) {
        auto& m = mods[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 76.f;

        Card(dl, cp, ImVec2(cp.x+cw, cp.y+ch), 18.f, C_CARD);

        ImGui::SetCursorScreenPos(ImVec2(cp.x+16, cp.y+12));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", m.name.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+16, cp.y+36));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f,0.64f,0.72f,1.f));
        ImGui::Text("v%s  |  %s  |  %s", m.version.c_str(), m.source.c_str(),
                    m.installed ? "installed" : "not installed");
        ImGui::PopStyleColor();

        float bx = cp.x + cw - 320.f;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + ch*0.5f - 14.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            m.enabled ? ImVec4(0.13f,0.77f,0.37f,1.f) : ImVec4(0.75f,0.35f,0.35f,1.f));
        ImGui::Text("%s", m.enabled ? "ON" : "OFF");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button(m.enabled?"Disable":"Enable", ImVec2(80,28))) {
            L.mods().setEnabled(i, !m.enabled, Settings::I().d().minecraftDir);
            FX::PlayFXSound(m.enabled ? FX::SND_UNCHECK : FX::SND_CHECK);
        }
        ImGui::SameLine();
        if (ImGui::Button("Update", ImVec2(80,28)))
            L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(80,28))) {
            L.mods().remove(i, Settings::I().d().minecraftDir);
            break;
        }

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + ch + 8.f));
    }
    ImGui::EndChild();
}

// ============================================================
//  iOS TOGGLE
// ============================================================
static bool IOSToggle(int slot, const char* id, bool value) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = 46.f, h = 26.f;
    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clk = ImGui::IsItemClicked();
    bool hov = ImGui::IsItemHovered();

    static float anim[8] = {0};
    float& a = anim[slot & 7];
    a = ExpSmooth(a, value ? 1.f : 0.f, 18.f);

    ImVec4 off = ImVec4(0.16f,0.20f,0.30f,1.f);
    ImVec4 on  = ImGui::ColorConvertU32ToFloat4(C_ACCENT);
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

    if (clk) FX::PlayFXSound(FX::SND_CLICK);
    return clk;
}

// ============================================================
//  PAGE: SETTINGS
// ============================================================
static void DrawSettingsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    (void)L;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& d = Settings::I().d();
    float pad = 24.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y), C_TEXT, "Settings");
    ImGui::PopFont();

    float cy = pos.y + 60.f;
    float cw = size.x;

    auto CardBlock = [&](float h) {
        ImVec2 a(pos.x, cy), b(pos.x+cw, cy+h);
        Card(dl, a, b, 18.f, C_CARD);
        ImGui::SetCursorScreenPos(ImVec2(a.x+16, a.y+12));
    };

    // Minecraft dir
    CardBlock(72);
    ImGui::Text("Minecraft Directory");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+38));
    static char dirB[512];
    strncpy_s(dirB, sizeof(dirB), d.minecraftDir.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(cw - 32);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::InputText("##dir", dirB, sizeof(dirB))) {
        d.minecraftDir = dirB;
        Settings::I().save();
    }
    ImGui::PopStyleVar();
    cy += 82.f;

    // Java path
    CardBlock(72);
    ImGui::Text("Java Path");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+38));
    static char jB[512];
    strncpy_s(jB, sizeof(jB), d.javaPath.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(cw - 180);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
    if (ImGui::InputText("##java", jB, sizeof(jB))) {
        d.javaPath = jB;
        Settings::I().save();
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (ImGui::Button("Auto Detect", ImVec2(130, 0))) {
        d.javaPath = Minecraft::FindJava();
        Settings::I().save();
        strncpy_s(jB, sizeof(jB), d.javaPath.c_str(), _TRUNCATE);
    }
    cy += 82.f;

    // RAM + window
    CardBlock(120);
    ImGui::Text("RAM (MB)");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+38));
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderInt("##ram", &d.ramMB, 512, 16384))
        Settings::I().save();

    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+72));
    ImGui::Text("Window");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+90, cy+72));
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputInt("##ww", &d.windowWidth, 0, 0)) Settings::I().save();
    ImGui::SameLine(); ImGui::Text("x"); ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputInt("##wh", &d.windowHeight, 0, 0)) Settings::I().save();
    cy += 130.f;

    // Toggles
    CardBlock(110);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Close launcher after launch");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+cw-16-46, cy+10));
    if (IOSToggle(0, "##t_close", d.closeAfterLaunch)) {
        d.closeAfterLaunch = !d.closeAfterLaunch;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+44));
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Debug Mode");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+cw-16-46, cy+40));
    if (IOSToggle(1, "##t_debug", d.debugMode)) {
        d.debugMode = !d.debugMode;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+16, cy+76));
    if (ImGui::Button("Open RavenXD Folder", ImVec2(200, 26))) {
        std::string p = Settings::I().appDataDir();
        ShellExecuteA(nullptr, "open", p.c_str(), nullptr, nullptr, SW_SHOW);
    }
    cy += 120.f;

    bool dc = DiscordRPC::I().isReady();
    ImGui::SetCursorScreenPos(ImVec2(pos.x, cy));
    ImGui::TextColored(dc ? ImVec4(0.13f,0.77f,0.37f,1.f)
                          : ImVec4(0.60f,0.64f,0.72f,1.f),
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
            o.hwndOwner   = GetActiveWindow();
            o.lpstrFilter = "Java\0javaw.exe;java.exe\0All\0*.*\0";
            o.lpstrFile   = f;
            o.nMaxFile    = MAX_PATH;
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
    dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, C_BG);
    const float pulse = 0.5f + 0.5f*sinf(g_timeNow * 0.65f);
    dl->AddCircleFilled(ImVec2(io.DisplaySize.x*0.78f, 120.f), 220.f, U32(24,90,180,(int)(10 + 8*pulse)));
    dl->AddCircleFilled(ImVec2(io.DisplaySize.x*0.48f, io.DisplaySize.y*0.94f), 260.f, U32(18,70,145,(int)(8 + 5*pulse)));

    float t = L.s().loadingTimer;
    float a = t < 0.6f ? t/0.6f : 1.f;
    float cx = io.DisplaySize.x*0.5f, cy = io.DisplaySize.y*0.5f;

    ImGui::PushFont(g_fontBig);
    ImVec2 ts = ImGui::CalcTextSize("RavenXD");
    ImGui::SetCursorScreenPos(ImVec2(cx-ts.x*0.5f, cy-16));
    ImGui::TextColored(ImVec4(1,1,1,a), "RavenXD");
    ImGui::PopFont();

    const char* ph = t < 0.8f ? "Checking files..." :
                     t < 1.6f ? "Loading launcher..." : "Ready";
    ImVec2 ts2 = ImGui::CalcTextSize(ph);
    dl->AddText(ImVec2(cx-ts2.x*0.5f, cy+30.f),
                U32(148,163,184,(int)(a*255)), ph);
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

    dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, C_BG);
    {
        float bp = 0.5f + 0.5f * sinf(g_timeNow * 0.7f);
        dl->AddCircleFilled(ImVec2(io.DisplaySize.x * 0.82f, 120.f), 210.f,
                            U32(26, 92, 190, (int)(8.f + 6.f * bp)));
        dl->AddCircleFilled(ImVec2(io.DisplaySize.x * 0.48f, io.DisplaySize.y * 0.96f), 240.f,
                            U32(25, 70, 150, (int)(6.f + 5.f * bp)));
    }

    {
        float sx = fmodf(g_timeNow * 90.f, io.DisplaySize.x + 420.f) - 210.f;
        dl->AddRectFilled(ImVec2(sx, 0.f), ImVec2(sx + 140.f, io.DisplaySize.y),
                          U32(255,255,255,3));
    }

    const float barH     = 64.f;
    const float sidebarW = 214.f;
    const float contentX = sidebarW;
    const float contentY = barH;
    const float contentW = io.DisplaySize.x - sidebarW;
    const float contentH = io.DisplaySize.y - barH;

    DrawSidebar(L, dl, ImVec2(0, barH), ImVec2(sidebarW, contentH));
    DrawTopBar(L, dl, io.DisplaySize);

    ImGui::SetNextWindowPos(ImVec2(contentX, contentY));
    ImGui::SetNextWindowSize(ImVec2(contentW, contentH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##content", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    const Page cur = L.s().page;
    int curIdx = 0;
    switch (cur) {
        case Page::Home:     curIdx=0; break;
        case Page::Accounts: curIdx=1; break;
        case Page::Mods:     curIdx=2; break;
        case Page::Settings: curIdx=3; break;
        default:             curIdx=0; break;
    }
    if (g_prevPage != curIdx) { g_prevPage = curIdx; g_pageAnim = 0.f; }
    g_pageAnim = ExpSmooth(g_pageAnim, 1.f, 14.f);
    float ease = EaseOutCubic(g_pageAnim);

    ImVec2 pagePos(30.f, 28.f + (1.f - ease) * 8.f);
    ImVec2 pageSize(contentW - 60.f, contentH - 56.f);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ease);

    switch (cur) {
        case Page::Home:     DrawHomePage    (L, pagePos, pageSize); break;
        case Page::Accounts: DrawAccountsPage(L, pagePos, pageSize); break;
        case Page::Mods:     DrawModsPage    (L, pagePos, pageSize); break;
        case Page::Settings: DrawSettingsPage(L, pagePos, pageSize); break;
        default: break;
    }
    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleVar();

    FX::DrawCursorPulse(dl);
    DrawJavaPopup(L);
}
