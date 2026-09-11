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
//  PALETTE  (iOS 17 dark, blue accent, glassy surfaces)
// ============================================================
static inline ImU32 U32(int r,int g,int b,int a=255){ return IM_COL32(r,g,b,a); }

static const ImU32 C_BG_TOP     = U32(  8, 12, 26);
static const ImU32 C_BG_BOT     = U32( 18, 26, 52);
static const ImU32 C_SURFACE    = U32( 26, 32, 52);
static const ImU32 C_SURFACE_2  = U32( 34, 42, 66);
static const ImU32 C_SURFACE_HI = U32( 46, 56, 84);
static const ImU32 C_STROKE     = U32(255,255,255, 28);
static const ImU32 C_STROKE_HI  = U32(255,255,255, 55);
static const ImU32 C_TEXT       = U32(245,248,255);
static const ImU32 C_TEXT_DIM   = U32(160,172,200);
static const ImU32 C_TEXT_DIM2  = U32(110,124,158);
static const ImU32 C_ACCENT     = U32( 10,132,255);   // iOS blue
static const ImU32 C_ACCENT_HI  = U32( 90,180,255);
static const ImU32 C_GREEN      = U32( 48,209, 88);   // iOS green
static const ImU32 C_RED        = U32(255, 69, 58);   // iOS red
static const ImU32 C_ORANGE     = U32(255,159, 10);

// ============================================================
//  STATE  (animation + UI globals)
// ============================================================
static char  g_newAccBuf[64] = "";
static float g_dt = 1.f/60.f;

static inline float ExpSmooth(float cur, float tgt, float sp) {
    float k = 1.f - expf(-sp * g_dt);
    return cur + (tgt - cur) * k;
}
static inline float EaseOutCubic(float t) { return 1.f - powf(1.f-t, 3.f); }
static inline float EaseInOut(float t)    { return t<0.5f ? 2*t*t : 1-powf(-2*t+2,2)/2; }

// iOS tab bar: 1 slot / tab. activeTabAnim = vị trí pill (theo index).
static int   g_tabCount = 5;
static float g_tabPillX   = 0.f;    // vị trí pill đang trượt
static float g_tabInit    = -1.f;   // lần đầu
static float g_tabHover[5] = {0,0,0,0,0};
static float g_tabScale[5] = {1,1,1,1,1};

// Trang: sheet trượt lên khi đổi tab (0 → 1)
static int   g_prevTab = -1;
static float g_sheetAnim = 1.f;

// Misc
static float g_toggleAnim[8] = {0};
static float g_dcHover = 0.f;
static float g_exitHover = 0.f;
static float g_launchPulse = 0.f;

// ============================================================
//  STYLE
// ============================================================
void UI::ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.f;
    s.ChildRounding  = 16.f;
    s.FrameRounding  = 12.f;
    s.PopupRounding  = 18.f;
    s.ScrollbarRounding = 12.f;
    s.GrabRounding   = 12.f;
    s.TabRounding    = 12.f;
    s.WindowBorderSize = 0.f;
    s.FrameBorderSize  = 0.f;
    s.PopupBorderSize  = 0.f;
    s.WindowPadding    = ImVec2(0,0);
    s.FramePadding     = ImVec2(14, 9);
    s.ItemSpacing      = ImVec2(10, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize    = 6.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0,0,0,0);
    c[ImGuiCol_ChildBg]         = ImVec4(0,0,0,0);
    c[ImGuiCol_PopupBg]         = ImVec4(0.10f,0.13f,0.20f,1.f);
    c[ImGuiCol_Border]          = ImVec4(1,1,1,0.10f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.13f,0.16f,0.24f,1.f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.18f,0.22f,0.32f,1.f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.22f,0.27f,0.38f,1.f);
    c[ImGuiCol_Button]          = ImVec4(0.14f,0.18f,0.28f,1.f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.20f,0.26f,0.40f,1.f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.26f,0.34f,0.52f,1.f);
    c[ImGuiCol_Header]          = ImVec4(0.18f,0.24f,0.40f,0.8f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.24f,0.32f,0.52f,1.f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.30f,0.40f,0.64f,1.f);
    c[ImGuiCol_Separator]       = ImVec4(1,1,1,0.08f);
    c[ImGuiCol_Text]            = ImVec4(0.96f,0.97f,1.f,1.f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.55f,0.60f,0.72f,1.f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.04f,0.52f,1.f,1.f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.04f,0.52f,1.f,1.f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.35f,0.70f,1.f,1.f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(1,1,1,0.18f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1,1,1,0.30f);
    c[ImGuiCol_ModalWindowDimBg]= ImVec4(0,0,0,0.55f);
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
        if (i==0)     f = ImDrawFlags_RoundCornersTop;
        if (i==N-1)   f = ImDrawFlags_RoundCornersBottom;
        dl->AddRectFilled(ImVec2(a.x,y0), ImVec2(b.x,y1), c, f?r:0.f, f);
    }
}

// "Kính mờ" giả lập: nền tối + viền sáng + highlight trên cùng
static void GlassPanel(ImDrawList* dl, ImVec2 a, ImVec2 b, float r,
                       ImU32 base = U32(28,34,54,235))
{
    dl->AddRectFilled(a, b, base, r);
    // highlight mảnh trên cùng
    dl->AddRectFilled(ImVec2(a.x+r*0.4f, a.y+1.f),
                      ImVec2(b.x-r*0.4f, a.y+2.f),
                      U32(255,255,255,22), 1.f);
    // viền
    dl->AddRect(a, b, C_STROKE, r, 0, 1.f);
}

// Nút tròn kính (top-right)
static bool GlassBtn(ImDrawList* dl, ImVec2 c, float rad,
                     void(*icon)(ImDrawList*,ImVec2,float,ImU32),
                     const char* id, float& hoverRef)
{
    ImGui::SetCursorScreenPos(ImVec2(c.x-rad, c.y-rad));
    ImGui::InvisibleButton(id, ImVec2(rad*2, rad*2));
    bool hov = ImGui::IsItemHovered();
    bool clk = ImGui::IsItemClicked();
    hoverRef = ExpSmooth(hoverRef, hov?1.f:0.f, 16.f);

    ImU32 base = U32(24,30,48,(int)(160 + 40*hoverRef));
    dl->AddCircleFilled(c, rad, base);
    dl->AddCircle(c, rad, U32(255,255,255,(int)(28 + 40*hoverRef)), 0, 1.2f);
    if (hoverRef > 0.01f)
        dl->AddCircleFilled(c, rad*0.92f, U32(255,255,255,(int)(10*hoverRef)));

    icon(dl, c, rad*0.80f, U32(255,255,255,(int)(210 + 45*hoverRef)));
    return clk;
}

// ============================================================
//  ICONS  (đơn giản, sạch, style iOS)
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
    dl->AddCircleFilled(ImVec2(c.x, c.y), s*0.22f, U32(26,32,52));
    for (int i=0;i<6;++i){
        float a = i*IM_PI/3.f;
        dl->AddLine(ImVec2(c.x+cosf(a)*s*0.5f, c.y+sinf(a)*s*0.5f),
                    ImVec2(c.x+cosf(a)*s*0.72f, c.y+sinf(a)*s*0.72f), col, 2.4f);
    }
}
static void I_Gear(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled(c, s*0.48f, col);
    dl->AddCircleFilled(c, s*0.20f, U32(26,32,52));
    for (int i=0;i<8;++i){
        float a = i*IM_PI/4.f;
        dl->AddLine(ImVec2(c.x+cosf(a)*s*0.48f, c.y+sinf(a)*s*0.48f),
                    ImVec2(c.x+cosf(a)*s*0.70f, c.y+sinf(a)*s*0.70f), col, 2.4f);
    }
}
static void I_Exit(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddRect(ImVec2(c.x-s*0.5f,c.y-s*0.5f), ImVec2(c.x+s*0.35f,c.y+s*0.5f),
                col, 2.5f, 0, 2.f);
    dl->AddLine(ImVec2(c.x-s*0.1f, c.y), ImVec2(c.x+s*0.6f, c.y), col, 2.f);
    dl->AddTriangleFilled(ImVec2(c.x+s*0.5f, c.y-s*0.22f),
        ImVec2(c.x+s*0.5f, c.y+s*0.22f), ImVec2(c.x+s*0.85f, c.y), col);
}
static void I_Chev(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddLine(ImVec2(c.x-s*0.25f, c.y-s*0.4f),
                ImVec2(c.x+s*0.25f, c.y), col, 2.f);
    dl->AddLine(ImVec2(c.x+s*0.25f, c.y),
                ImVec2(c.x-s*0.25f, c.y+s*0.4f), col, 2.f);
}

// ============================================================
//  iOS TOGGLE
// ============================================================
static bool IOSToggle(int slot, const char* id, bool value) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = 50.f, h = 30.f;
    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clk = ImGui::IsItemClicked();
    bool hov = ImGui::IsItemHovered();

    float& a = g_toggleAnim[slot & 7];
    a = ExpSmooth(a, value ? 1.f : 0.f, 18.f);

    ImVec4 off = ImVec4(0.20f, 0.22f, 0.28f, 1.f);
    ImVec4 on  = ImGui::ColorConvertU32ToFloat4(C_GREEN);
    ImU32 track = ImGui::GetColorU32(ImLerp(off, on, a));
    if (hov) track = ImGui::GetColorU32(ImLerp(ImGui::ColorConvertU32ToFloat4(track),
                                               ImVec4(1,1,1,1), 0.06f));

    float r = h*0.5f;
    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), track, r);

    float kr = r - 2.5f;
    float kx = p.x + r + a * (w - h);
    float ky = p.y + r;
    dl->AddCircleFilled(ImVec2(kx, ky+1.5f), kr+1.f, U32(0,0,0,60));
    dl->AddCircleFilled(ImVec2(kx, ky), kr, U32(255,255,255));
    return clk;
}

// ============================================================
//  iOS TAB BAR  (KHÔNG bao giờ bị che - submit trong cùng window)
// ============================================================
typedef void(*IconFn)(ImDrawList*, ImVec2, float, ImU32);

static void DrawTabBar(Launcher& L, ImDrawList* dl, ImVec2 disp)
{
    struct Tab { Page page; IconFn icon; const char* label; };
    static const Tab tabs[5] = {
        { Page::Home,     I_Home,   "Home"     },
        { Page::Accounts, I_User,   "Accounts" },
        { Page::Versions, I_Grid,   "Versions" },
        { Page::Mods,     I_Puzzle, "Mods"     },
        { Page::Settings, I_Gear,   "Settings" },
    };

    const float barH   = 68.f;
    const float barW   = 560.f;
    const float barX   = (disp.x - barW) * 0.5f;
    const float barY   = disp.y - barH - 18.f;
    const float slotW  = barW / 5.f;

    // Nền glass pill
    ImVec2 a(barX, barY), b(barX+barW, barY+barH);
    dl->AddRectFilled(ImVec2(a.x+2, a.y+4), ImVec2(b.x+2, b.y+6), U32(0,0,0,80), barH*0.5f);
    GlassPanel(dl, a, b, barH*0.5f, U32(22,28,44,235));

    // Active pill trượt
    int activeIdx = 0;
    for (int i=0;i<5;++i) if (tabs[i].page == L.s().page) activeIdx = i;

    float targetPillX = barX + activeIdx*slotW + slotW*0.5f - slotW*0.44f;
    if (g_tabInit < 0.f) { g_tabInit = 1.f; g_tabPillX = targetPillX; }
    g_tabPillX = ExpSmooth(g_tabPillX, targetPillX, 14.f);

    ImVec2 pa(g_tabPillX, a.y + 6.f);
    ImVec2 pb(g_tabPillX + slotW*0.88f, b.y - 6.f);
    GradV(dl, pa, pb, C_ACCENT_HI, C_ACCENT, (pb.y-pa.y)*0.5f);

    // Vẽ + xử lý click từng tab
    for (int i=0;i<5;++i) {
        ImVec2 tp(barX + i*slotW, a.y);
        ImGui::SetCursorScreenPos(tp);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##tab", ImVec2(slotW, barH));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        ImGui::PopID();

        g_tabHover[i] = ExpSmooth(g_tabHover[i], hov?1.f:0.f, 16.f);
        float tScale = clk ? 0.86f : (hov ? 1.08f : 1.f);
        g_tabScale[i] = ExpSmooth(g_tabScale[i], tScale, 22.f);

        bool active = (i == activeIdx);
        ImVec2 c(tp.x + slotW*0.5f, tp.y + barH*0.38f);

        ImU32 iconCol = active ? U32(255,255,255)
                               : U32(150,162,195,(int)(200 + 55*g_tabHover[i]));
        tabs[i].icon(dl, c, 20.f * g_tabScale[i], iconCol);

        ImGui::PushFont(g_fontRegular);
        const char* lbl = tabs[i].label;
        ImVec2 sz = ImGui::CalcTextSize(lbl);
        ImU32 lblCol = active ? U32(255,255,255) : U32(150,162,195, 220);
        dl->AddText(ImVec2(tp.x + (slotW - sz.x)*0.5f, tp.y + barH - 22.f),
                    lblCol, lbl);
        ImGui::PopFont();

        if (clk && !active) L.s().page = tabs[i].page;
    }
}

// ============================================================
//  TOP BAR  (logo + discord + exit)
// ============================================================
static void DrawTopBar(ImDrawList* dl, ImVec2 disp)
{
    ensureLogo();
    float ls = 28.f;
    ImVec2 lp(24.f, 24.f);
    if (g_logoTex) {
        dl->AddImage((ImTextureID)g_logoTex, lp, ImVec2(lp.x+ls, lp.y+ls));
        ImGui::PushFont(g_fontBold);
        dl->AddText(ImVec2(lp.x+ls+10, lp.y+4), U32(255,255,255), "RavenXD");
        ImGui::PopFont();
    } else {
        ImGui::PushFont(g_fontBold);
        dl->AddText(lp, U32(255,255,255), "RavenXD");
        ImGui::PopFont();
    }

    // Exit
    float r = 19.f;
    ImVec2 exitC(disp.x - 22.f - r, 22.f + r);
    if (GlassBtn(dl, exitC, r, I_Exit, "##exit", g_exitHover))
        PostQuitMessage(0);

    // Discord status
    ImVec2 dcC(exitC.x - (r*2 + 10.f), exitC.y);
    ImGui::SetCursorScreenPos(ImVec2(dcC.x-r, dcC.y-r));
    ImGui::InvisibleButton("##dc", ImVec2(r*2, r*2));
    bool hov = ImGui::IsItemHovered();
    g_dcHover = ExpSmooth(g_dcHover, hov?1.f:0.f, 16.f);
    bool ok = DiscordRPC::I().isReady();
    dl->AddCircleFilled(dcC, r, U32(24,30,48,(int)(160 + 40*g_dcHover)));
    dl->AddCircle(dcC, r, U32(255,255,255,(int)(28 + 40*g_dcHover)), 0, 1.2f);
    dl->AddCircleFilled(dcC, 6.f, ok ? C_GREEN : U32(120,120,130));
    if (hov) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ok ? ImVec4(0.19f,0.82f,0.35f,1) : ImVec4(0.6f,0.6f,0.65f,1),
                           ok ? "Discord: Connected" : "Discord: Not running");
        ImGui::EndTooltip();
    }
}

// ============================================================
//  PAGE: HOME (hero + launch card)
// ============================================================
static void DrawHomePage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Hero title
    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x, pos.y + 10.f), U32(255,255,255,235), "RavenXD");
    ImGui::PopFont();
    ImGui::PushFont(g_fontRegular);
    dl->AddText(ImVec2(pos.x, pos.y + 62.f), U32(180,196,225,220),
                "Minecraft 1.8.9  •  Forge  •  OptiFine");
    ImGui::PopFont();

    // Launch card
    float cardW = size.x < 760.f ? size.x : 760.f;
    float cardH = 250.f;
    ImVec2 c0(pos.x + (size.x-cardW)*0.5f, pos.y + 110.f);
    ImVec2 c1(c0.x + cardW, c0.y + cardH);

    GlassPanel(dl, c0, c1, 24.f, U32(26,32,52,225));

    // Profile row
    float pad = 24.f;
    int ai = AccountManager::I().activeIndex();
    std::string an = AccountManager::I().activeName();
    int colorIdx = 0;
    if (ai >= 0 && ai < (int)AccountManager::I().list().size())
        colorIdx = AccountManager::I().list()[ai].colorIdx;

    // Avatar
    int rr,gg,bb; AccountManager::avatarColor(colorIdx, rr,gg,bb);
    ImVec2 avC(c0.x + pad + 22, c0.y + pad + 22);
    dl->AddCircleFilled(avC, 22.f, U32(rr,gg,bb));
    {
        std::string ini = an.empty() ? "P" : std::string(1, toupper((unsigned char)an[0]));
        ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
        ImVec2 ts = f->CalcTextSizeA(24.f, FLT_MAX, 0, ini.c_str());
        ImGui::PushFont(f);
        dl->AddText(f, 24.f, ImVec2(avC.x-ts.x*0.5f, avC.y-ts.y*0.5f),
                    U32(255,255,255), ini.c_str());
        ImGui::PopFont();
    }
    dl->AddCircle(avC, 23.f, C_GREEN, 0, 2.f);

    ImGui::PushFont(g_fontBold);
    dl->AddText(ImVec2(avC.x+34, c0.y+pad+6), U32(255,255,255), an.c_str());
    ImGui::PopFont();
    dl->AddText(ImVec2(avC.x+34, c0.y+pad+30), C_TEXT_DIM,
                "Tap Accounts to switch profile");

    // Version combo
    float verY = c0.y + pad + 62.f;
    const char* versions[] = { "Minecraft 1.8.9", "Forge 1.8.9", "Forge 1.8.9 + OptiFine" };
    float launchW = 150.f, launchH = 44.f;
    float comboW = c1.x - c0.x - pad*2 - launchW - 14.f;

    ImGui::SetCursorScreenPos(ImVec2(c0.x+pad, verY));
    ImGui::SetNextItemWidth(comboW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    ImGui::Combo("##ver", &L.s().selectedVersion, versions, 3);
    ImGui::PopStyleVar();

    // Launch button
    bool busy = L.s().taskState == TaskState::Running;
    ImVec2 lb(c1.x - pad - launchW, verY - 1.f);

    g_launchPulse += g_dt;
    float pulse = 0.5f + 0.5f*sinf(g_launchPulse*2.4f);
    int glowA = busy ? 30 : (int)(30 + 30*pulse);
    dl->AddRectFilled(ImVec2(lb.x-5,lb.y-5),
                      ImVec2(lb.x+launchW+5, lb.y+launchH+5),
                      U32(10,132,255,glowA), 15.f);

    ImGui::SetCursorScreenPos(lb);
    ImGui::PushFont(g_fontBold);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.04f,0.52f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f,0.66f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.02f,0.42f,0.86f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.f);
    if (!busy) {
        if (ImGui::Button("LAUNCH", ImVec2(launchW, launchH))) L.onLaunchClicked();
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("...", ImVec2(launchW, launchH));
        ImGui::EndDisabled();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PopFont();

    // Progress
    float progY = verY + 60.f;
    ImGui::SetCursorScreenPos(ImVec2(c0.x+pad, progY));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.04f,0.52f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.10f,0.13f,0.20f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.f);
    static float shown = 0.f;
    shown = ExpSmooth(shown, L.s().progress, 8.f);
    char ov[80];
    if (L.s().speedMBps > 0.f)
        snprintf(ov, sizeof(ov), "%d%%   %.2f MB/s", (int)(shown*100.f), L.s().speedMBps);
    else
        snprintf(ov, sizeof(ov), "%d%%", (int)(shown*100.f));
    ImGui::ProgressBar(shown, ImVec2(c1.x - c0.x - pad*2, 16), ov);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // Status
    float sy = progY + 30.f;
    dl->AddText(ImVec2(c0.x+pad, sy), C_TEXT_DIM, L.s().statusText.c_str());
    if (L.s().taskState == TaskState::Failed && !L.s().lastError.empty()) {
        std::string err = "Error: " + L.s().lastError;
        dl->AddText(ImVec2(c0.x+pad, sy+20.f), C_RED, err.c_str());
    }
}

// ============================================================
//  PAGE: ACCOUNTS
// ============================================================
static void DrawAccountsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 24.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x+pad, pos.y+8), U32(255,255,255), "Accounts");
    ImGui::PopFont();
    dl->AddText(ImVec2(pos.x+pad, pos.y+62), C_TEXT_DIM,
                "Add offline accounts and pick which one to launch with");

    // Add new
    float addY = pos.y + 100.f;
    ImVec2 a0(pos.x+pad, addY), a1(pos.x+size.x-pad, addY+66.f);
    GlassPanel(dl, a0, a1, 16.f, U32(26,32,52,220));

    ImGui::SetCursorScreenPos(ImVec2(a0.x+18, a0.y+12));
    ImGui::Text("New account");
    ImGui::SetCursorScreenPos(ImVec2(a0.x+18, a0.y+36));
    ImGui::SetNextItemWidth(280);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::InputText("##newacc", g_newAccBuf, sizeof(g_newAccBuf),
                     ImGuiInputTextFlags_CharsNoBlank);
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (ImGui::Button("+ Add", ImVec2(120, 0))) {
        if (strlen(g_newAccBuf) > 0) {
            AccountManager::I().add(g_newAccBuf);
            g_newAccBuf[0] = 0;
        }
    }

    // List
    float listY = addY + 80.f;
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, listY));
    ImGui::BeginChild("##acclist", ImVec2(size.x-pad*2, size.y-(listY-pos.y)-pad), false);

    auto& list = AccountManager::I().list();
    for (size_t i=0;i<list.size();++i) {
        auto& acc = list[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 78.f;

        ImU32 base = acc.active ? U32(38, 60, 108, 235) : U32(26,32,52,220);
        GlassPanel(dl, cp, ImVec2(cp.x+cw, cp.y+ch), 16.f, base);

        // Avatar
        int rr,gg,bb; AccountManager::avatarColor(acc.colorIdx, rr,gg,bb);
        ImVec2 av(cp.x+42, cp.y+ch*0.5f);
        dl->AddCircleFilled(av, 22.f, U32(rr,gg,bb));
        std::string ini = acc.name.empty() ? "P" : std::string(1, toupper((unsigned char)acc.name[0]));
        ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
        ImVec2 ts = f->CalcTextSizeA(22.f, FLT_MAX, 0, ini.c_str());
        ImGui::PushFont(f);
        dl->AddText(f, 22.f, ImVec2(av.x-ts.x*0.5f, av.y-ts.y*0.5f),
                    U32(255,255,255), ini.c_str());
        ImGui::PopFont();
        if (acc.active) dl->AddCircle(av, 23.f, C_GREEN, 0, 2.f);

        // Name + status
        ImGui::SetCursorScreenPos(ImVec2(cp.x+80, cp.y+14));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", acc.name.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+80, cp.y+38));
        ImGui::PushStyleColor(ImGuiCol_Text,
            acc.active ? ImVec4(0.19f,0.82f,0.35f,1.f) : ImVec4(0.55f,0.62f,0.75f,1.f));
        ImGui::Text("%s", acc.active ? "● Active" : "○ Offline");
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+80, cp.y+56));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f,0.50f,0.68f,1.f));
        std::string uu = acc.uuid.substr(0, 20) + "...";
        ImGui::Text("%s", uu.c_str());
        ImGui::PopStyleColor();

        // Buttons
        float bx = cp.x + cw - 340.f;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + ch*0.5f - 15.f));
        if (!acc.active) {
            if (ImGui::Button("Set Active", ImVec2(100, 30)))
                AccountManager::I().setActive(i);
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("Current", ImVec2(100, 30));
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Rename", ImVec2(90, 30))) {
            std::string nn = acc.name + "_2";
            AccountManager::I().rename(i, nn);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(90, 30))) {
            AccountManager::I().remove(i);
            break;
        }

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + ch + 10));
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

static float g_verHover[4] = {0,0,0,0};

static void DrawVersionsPage(Launcher& L, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 24.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x+pad, pos.y+8), U32(255,255,255), "Versions");
    ImGui::PopFont();
    dl->AddText(ImVec2(pos.x+pad, pos.y+62), C_TEXT_DIM,
                "Pick a client to install & launch");

    float gridY = pos.y + 110.f;
    float gap = 16.f;
    int cols = 3;
    float cardW = (size.x - pad*2 - gap*(cols-1)) / cols;
    float cardH = 170.f;

    for (int i=0;i<4;++i) {
        int row = i/cols, col = i%cols;
        ImVec2 cp(pos.x+pad + col*(cardW+gap), gridY + row*(cardH+gap));
        if (cp.x + cardW > pos.x + size.x - pad) continue;

        ImGui::SetCursorScreenPos(cp);
        ImGui::InvisibleButton(g_versions[i].title, ImVec2(cardW, cardH));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        g_verHover[i] = ExpSmooth(g_verHover[i], hov?1.f:0.f, 12.f);

        float grow = g_verHover[i]*4.f;
        ImVec2 g0(cp.x-grow*0.5f, cp.y-grow*0.5f);
        ImVec2 g1(cp.x+cardW+grow*0.5f, cp.y+cardH+grow*0.5f);

        GradV(dl, g0, g1, g_versions[i].c1, g_versions[i].c2, 16.f);
        dl->AddRectFilled(g0, g1, U32(0,0,0,(int)(100 - 40*g_verHover[i])), 16.f);

        bool sel = (L.s().selectedVersion == g_versions[i].mapSel);
        if (sel)
            dl->AddRect(g0, g1, g_versions[i].accent, 16.f, 0, 3.f);
        else if (g_verHover[i] > 0.01f)
            dl->AddRect(g0, g1, U32(255,255,255,(int)(200*g_verHover[i])), 16.f, 0, 2.f);

        // Tag
        const char* tg = g_versions[i].tag;
        ImVec2 tsz = ImGui::CalcTextSize(tg);
        ImVec2 tp(g1.x - tsz.x - 26, g0.y + 12);
        dl->AddRectFilled(tp, ImVec2(tp.x+tsz.x+18, tp.y+tsz.y+8),
                          g_versions[i].free ? U32(48,209,88) : U32(0,0,0,170), 8.f);
        dl->AddText(ImVec2(tp.x+9, tp.y+4), U32(255,255,255), tg);

        // Title
        ImGui::PushFont(g_fontBold);
        ImVec2 tt = ImGui::CalcTextSize(g_versions[i].title);
        ImVec2 tpos(g0.x+16, g1.y - tt.y - 34.f);
        dl->AddText(ImVec2(tpos.x+1,tpos.y+1), U32(0,0,0,200), g_versions[i].title);
        dl->AddText(tpos, U32(255,255,255), g_versions[i].title);
        ImGui::PopFont();

        ImVec2 ss = ImGui::CalcTextSize(g_versions[i].sub);
        dl->AddText(ImVec2(g0.x+16, g1.y - ss.y - 12.f),
                    U32(230,235,245,210), g_versions[i].sub);

        // Chev
        I_Chev(dl, ImVec2(g1.x-22, g1.y-22), 12.f, U32(255,255,255,230));

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
    float pad = 24.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x+pad, pos.y+8), U32(255,255,255), "Mods");
    ImGui::PopFont();

    // Update all (top right)
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - pad - 130, pos.y + 14));
    if (ImGui::Button("Update All", ImVec2(130, 34)))
        L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, pos.y + 70.f));
    ImGui::BeginChild("##modscroll",
        ImVec2(size.x-pad*2, size.y - 70.f - pad), false);

    auto& mods = L.mods().mods();
    for (size_t i=0;i<mods.size();++i) {
        auto& m = mods[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 84.f;

        GlassPanel(dl, cp, ImVec2(cp.x+cw, cp.y+ch), 16.f, U32(26,32,52,220));

        ImGui::SetCursorScreenPos(ImVec2(cp.x+18, cp.y+14));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", m.name.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+18, cp.y+40));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.62f,0.75f,1.f));
        ImGui::Text("v%s  •  %s  •  %s", m.version.c_str(), m.source.c_str(),
                    m.installed ? "installed" : "not installed");
        ImGui::PopStyleColor();

        float bx = cp.x + cw - 320.f;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + ch*0.5f - 15.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            m.enabled ? ImVec4(0.19f,0.82f,0.35f,1.f) : ImVec4(0.75f,0.35f,0.35f,1.f));
        ImGui::Text("%s", m.enabled ? "ON" : "OFF");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button(m.enabled?"Disable":"Enable", ImVec2(80,30)))
            L.mods().setEnabled(i, !m.enabled, Settings::I().d().minecraftDir);
        ImGui::SameLine();
        if (ImGui::Button("Update", ImVec2(80,30)))
            L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(80,30)))
            L.mods().remove(i, Settings::I().d().minecraftDir);

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + ch + 10.f));
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
    float pad = 24.f;

    ImGui::PushFont(g_fontBig);
    dl->AddText(ImVec2(pos.x+pad, pos.y+8), U32(255,255,255), "Settings");
    ImGui::PopFont();

    float cy = pos.y + 70.f;
    float cw = size.x - pad*2;

    auto Card = [&](float h) {
        ImVec2 a(pos.x+pad, cy), b(pos.x+pad+cw, cy+h);
        GlassPanel(dl, a, b, 16.f, U32(26,32,52,215));
        ImGui::SetCursorScreenPos(ImVec2(a.x+18, a.y+14));
    };

    // --- Minecraft dir ---
    Card(78);
    ImGui::Text("Minecraft Directory");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+42));
    static char dirB[512];
    strncpy_s(dirB, d.minecraftDir.c_str(), sizeof(dirB)-1);
    ImGui::SetNextItemWidth(cw - 36);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    if (ImGui::InputText("##dir", dirB, sizeof(dirB))) {
        d.minecraftDir = dirB; Settings::I().save();
    }
    ImGui::PopStyleVar();
    cy += 88.f;

    // --- Java path ---
    Card(78);
    ImGui::Text("Java Path");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+42));
    static char jB[512];
    strncpy_s(jB, d.javaPath.c_str(), sizeof(jB)-1);
    ImGui::SetNextItemWidth(cw - 200);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    if (ImGui::InputText("##java", jB, sizeof(jB))) {
        d.javaPath = jB; Settings::I().save();
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (ImGui::Button("Auto Detect", ImVec2(140, 0))) {
        d.javaPath = Minecraft::FindJava();
        Settings::I().save();
        strncpy_s(jB, d.javaPath.c_str(), sizeof(jB)-1);
    }
    cy += 88.f;

    // --- RAM + window ---
    Card(130);
    ImGui::Text("RAM (MB)");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+42));
    ImGui::SetNextItemWidth(240);
    if (ImGui::SliderInt("##ram", &d.ramMB, 512, 16384)) Settings::I().save();

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+80));
    ImGui::Text("Window");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+95, cy+80));
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##ww", &d.windowWidth, 0, 0)) Settings::I().save();
    ImGui::SameLine(); ImGui::Text("×"); ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##wh", &d.windowHeight, 0, 0)) Settings::I().save();
    cy += 140.f;

    // --- Toggles ---
    Card(126);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Close launcher after launch");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+cw-18-50, cy+11));
    if (IOSToggle(0, "##t_close", d.closeAfterLaunch)) {
        d.closeAfterLaunch = !d.closeAfterLaunch;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+50));
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Debug Mode");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+cw-18-50, cy+46));
    if (IOSToggle(1, "##t_debug", d.debugMode)) {
        d.debugMode = !d.debugMode;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+86));
    if (ImGui::Button("Open RavenXD Folder", ImVec2(210, 30))) {
        std::string p = Settings::I().appDataDir();
        ShellExecuteA(nullptr, "open", p.c_str(), nullptr, nullptr, SW_SHOW);
    }
    cy += 138.f;

    // --- Discord status ---
    bool dc = DiscordRPC::I().isReady();
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, cy));
    ImGui::TextColored(dc ? ImVec4(0.19f,0.82f,0.35f,1.f)
                          : ImVec4(0.55f,0.60f,0.70f,1.f),
                       dc ? "● Discord: Connected" : "○ Discord: Not running");
}

// ============================================================
//  JAVA POPUP
// ============================================================
static void DrawJavaPopup(Launcher& L)
{
    if (L.s().showJavaPopup) ImGui::OpenPopup("Java");
    ImGui::SetNextWindowSize(ImVec2(440, 0));
    if (ImGui::BeginPopupModal("Java", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(g_fontBold);
        ImGui::TextColored(ImVec4(1.f,0.4f,0.4f,1.f), "Java not found");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::TextWrapped("RavenXD couldn't locate a Java 8 installation. "
                           "Pick javaw.exe manually or install Java 8 (Temurin).");
        ImGui::Dummy(ImVec2(0, 8));
        if (ImGui::Button("Select Java", ImVec2(160, 36))) {
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
        if (ImGui::Button("Get Java 8", ImVec2(120, 36)))
            ShellExecuteA(nullptr, "open",
                "https://adoptium.net/temurin/releases/?version=8",
                nullptr, nullptr, SW_SHOW);
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80, 36))) {
            L.s().showJavaPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================
//  LOADING SCREEN
// ============================================================
void UI::RenderLoadingScreen(Launcher& L)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, C_BG_TOP);

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
    dl->AddText(ImVec2(cx-ts.x*0.5f, cy+90.f), U32(170,185,215,(int)(a*255)), ph);
}

// ============================================================
//  ROOT  —  CHỈ 1 WINDOW DUY NHẤT  =>  KHÔNG BAO GIỜ KẸT TAB
// ============================================================
void UI::Render(Launcher& L, float dt)
{
    g_dt = dt > 0.f ? dt : (1.f/60.f);
    L.tick(dt);

    if (L.s().showLoadingScreen) { RenderLoadingScreen(L); return; }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // ---------- Background ----------
    ensureBg();
    if (g_bgTex) {
        dl->AddImage((ImTextureID)g_bgTex, ImVec2(0,0), io.DisplaySize,
                     ImVec2(0,0), ImVec2(1,1), U32(255,255,255));
        dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, U32(4,8,16,200));
    } else {
        GradV(dl, ImVec2(0,0), io.DisplaySize, C_BG_TOP, C_BG_BOT, 0.f);
    }

    // ---------- Sheet animation khi đổi tab ----------
    const Page cur = L.s().page;
    int curIdx = 0;
    switch (cur) {
        case Page::Home: curIdx=0; break;
        case Page::Accounts: curIdx=1; break;
        case Page::Versions: curIdx=2; break;
        case Page::Mods: curIdx=3; break;
        case Page::Settings: curIdx=4; break;
    }
    if (g_prevTab != curIdx) { g_prevTab = curIdx; g_sheetAnim = 0.f; }
    g_sheetAnim = ExpSmooth(g_sheetAnim, 1.f, 12.f);
    float sheetEase = EaseOutCubic(g_sheetAnim);

    // ---------- Vùng nội dung (giữa top bar và tab bar) ----------
    const float topBarH  = 76.f;
    const float tabBarH  = 68.f + 18.f + 20.f;  // bar + bottom margin + pad
    ImVec2 contentPos(0.f, topBarH);
    ImVec2 contentSize(io.DisplaySize.x,
                       io.DisplaySize.y - topBarH - tabBarH);

    // ---------- 1 WINDOW DUY NHẤT ----------
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollWithMouse);

    // --- Nội dung trang (có animation sheet) ---
    {
        float offsetY = (1.f - sheetEase) * 30.f;
        float alpha   = sheetEase;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        ImVec2 pp(contentPos.x, contentPos.y + offsetY);
        ImVec2 ps(contentSize.x, contentSize.y);

        switch (cur) {
            case Page::Home:     DrawHomePage    (L, ImVec2(pp.x+24, pp.y), ImVec2(ps.x-48, ps.y)); break;
            case Page::Accounts: DrawAccountsPage(L, pp, ps); break;
            case Page::Versions: DrawVersionsPage(L, pp, ps); break;
            case Page::Mods:     DrawModsPage    (L, pp, ps); break;
            case Page::Settings: DrawSettingsPage(L, pp, ps); break;
        }
        ImGui::PopStyleVar();
    }

    // --- Top bar ---
    DrawTopBar(dl, io.DisplaySize);

    // --- Tab bar (submit CUỐI CÙNG trong cùng window → luôn nhận click) ---
    DrawTabBar(L, dl, io.DisplaySize);

    ImGui::End();
    ImGui::PopStyleVar();

    DrawJavaPopup(L);
}
