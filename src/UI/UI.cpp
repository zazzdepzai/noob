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

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dDeviceContext;
extern ImFont*                 g_fontRegular;
extern ImFont*                 g_fontBold;
extern ImFont*                 g_fontBig;
extern void LoadTextureFromMemory(const unsigned char* data, size_t len,
                                  ID3D11ShaderResourceView** out);

ID3D11ShaderResourceView*      g_logoTex = nullptr;
static ID3D11ShaderResourceView* g_bgTex = nullptr;
static bool                    g_logoTried = false;
static bool                    g_bgTried    = false;

// ======================= PALETTE =======================
static ImU32 U32(int r, int g, int b, int a=255){ return IM_COL32(r,g,b,a); }

static const ImU32 COL_BG          = U32(8, 12, 24);
static const ImU32 COL_BG_BOT      = U32(16, 24, 48);
static const ImU32 COL_PANEL       = U32(16, 22, 40);
static const ImU32 COL_PANEL_BOT   = U32(11, 16, 32);
static const ImU32 COL_CARD        = U32(24, 34, 58);
static const ImU32 COL_CARD_BOT    = U32(18, 26, 46);
static const ImU32 COL_CARD_HOV    = U32(34, 48, 80);
static const ImU32 COL_TEXT        = U32(245, 248, 255);
static const ImU32 COL_TEXT_DIM    = U32(160, 178, 210);
static const ImU32 COL_TEXT_DIMMER = U32(110, 128, 165);
static const ImU32 COL_ACCENT      = U32(64, 140, 245);
static const ImU32 COL_ACCENT_HI   = U32(120, 175, 255);
static const ImU32 COL_ACCENT_LO   = U32(40, 95, 190);
static const ImU32 COL_GREEN       = U32(80, 210, 130);

// ======================= STATE =======================
static char g_newAccBuf[64] = "";

// ======================= ANIMATION HELPERS =======================
// Smoothing/easing infrastructure used throughout the UI to make hovers,
// selections and progress feel like iOS-style spring/ease transitions
// instead of the previous instant on/off states.
static float g_dt = 1.f/60.f;

// Frame-rate independent exponential smoothing: current -> target.
// Higher "speed" = snappier animation.
static inline float ExpSmooth(float current, float target, float speed) {
    float t = 1.f - expf(-speed * g_dt);
    return current + (target - current) * t;
}

// Per-widget animation state (small fixed pools; indices are assigned by hand
// at each call site, matching the always-in-the-same-order UI layout).
static float g_sbHover[6]   = {0,0,0,0,0,0}; // sidebar: Home,Accounts,Versions,Mods,Settings,Exit
static float g_sbIndicatorY = -1.f;          // sliding active-tab pill (sidebar)
static float g_dcHover      = 0.f;           // discord status dot hover
static float g_verHover[4]  = {0,0,0,0};     // version cards
static float g_toggleAnim[4]= {0,0,0,0};     // iOS-style toggle switches
static float g_launchGlowT  = 0.f;           // launch button breathing glow clock

// ======================= STYLE =======================
void UI::ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 20.f;
    s.ChildRounding  = 18.f;
    s.FrameRounding  = 14.f;
    s.PopupRounding  = 18.f;
    s.ScrollbarRounding = 16.f;
    s.GrabRounding   = 14.f;
    s.TabRounding    = 14.f;
    s.WindowBorderSize = 0.f;
    s.FrameBorderSize  = 0.f;
    s.PopupBorderSize  = 0.f;
    s.WindowPadding    = ImVec2(0,0);
    s.FramePadding     = ImVec2(14, 10);
    s.ItemSpacing      = ImVec2(10, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize    = 8.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.03f,0.05f,0.09f,1.f);
    c[ImGuiCol_ChildBg]          = ImVec4(0.f,0.f,0.f,0.f);
    c[ImGuiCol_PopupBg]          = ImVec4(0.07f,0.10f,0.18f,1.f);
    c[ImGuiCol_Border]           = ImVec4(0.30f,0.42f,0.65f,0.35f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.09f,0.13f,0.22f,1.f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.14f,0.20f,0.34f,1.f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.18f,0.26f,0.44f,1.f);
    c[ImGuiCol_Button]           = ImVec4(0.16f,0.24f,0.42f,1.f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(0.24f,0.36f,0.60f,1.f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.30f,0.44f,0.72f,1.f);
    c[ImGuiCol_Header]           = ImVec4(0.20f,0.30f,0.50f,0.7f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.26f,0.40f,0.64f,0.9f);
    c[ImGuiCol_HeaderActive]     = ImVec4(0.32f,0.48f,0.76f,1.f);
    c[ImGuiCol_Separator]        = ImVec4(0.20f,0.30f,0.50f,0.30f);
    c[ImGuiCol_Text]             = ImVec4(0.96f,0.97f,1.f,1.f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.55f,0.62f,0.75f,1.f);
    c[ImGuiCol_CheckMark]        = ImVec4(0.25f,0.55f,0.95f,1.f);
    c[ImGuiCol_SliderGrab]       = ImVec4(0.25f,0.55f,0.95f,1.f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.40f,0.68f,1.f,1.f);
    c[ImGuiCol_ScrollbarBg]      = ImVec4(0.f,0.f,0.f,0.f);
    c[ImGuiCol_ScrollbarGrab]    = ImVec4(0.22f,0.32f,0.52f,1.f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f,0.44f,0.68f,1.f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f,0.0f,0.0f,0.70f);
}

// ======================= TEXTURES =======================
static void ensureLogo() {
    if (g_logoTried) return;
    g_logoTried = true;
    if (RAVENXD_LOGO_PNG_SIZE > 4)
        LoadTextureFromMemory(RAVENXD_LOGO_PNG, RAVENXD_LOGO_PNG_SIZE, &g_logoTex);
}
static void ensureBg() {
    if (g_bgTried) return;
    g_bgTried = true;
    if (RAVENXD_BG_PNG_SIZE > 4)
        LoadTextureFromMemory(RAVENXD_BG_PNG, RAVENXD_BG_PNG_SIZE, &g_bgTex);
}

// ======================= GRADIENT =======================
static void DrawGradientV(ImDrawList* dl, ImVec2 p1, ImVec2 p2,
                          ImU32 colTop, ImU32 colBot, float rounding)
{
    ImVec4 cT = ImGui::ColorConvertU32ToFloat4(colTop);
    ImVec4 cB = ImGui::ColorConvertU32ToFloat4(colBot);
    int steps = 32;
    float h = p2.y - p1.y;
    float r = rounding;
    for (int i = 0; i < steps; ++i) {
        float t = (float)i / steps;
        ImVec4 c = ImLerp(cT, cB, t);
        ImU32 cc = ImGui::GetColorU32(c);
        float y0 = p1.y + h*t;
        float y1 = p1.y + h*(t + 1.f/steps);
        ImDrawFlags flags = 0;
        if (i == 0) flags = ImDrawFlags_RoundCornersTop;
        else if (i == steps-1) flags = ImDrawFlags_RoundCornersBottom;
        dl->AddRectFilled(ImVec2(p1.x, y0), ImVec2(p2.x, y1), cc,
                          flags ? r : 0.f, flags);
    }
    if (r > 0.5f) {
        dl->AddRectFilled(p1, ImVec2(p2.x, p1.y + r), colTop, r, ImDrawFlags_RoundCornersTop);
        dl->AddRectFilled(ImVec2(p1.x, p2.y - r), p2, colBot, r, ImDrawFlags_RoundCornersBottom);
    }
}

// ======================= ICONS =======================
static void IconHome(ImDrawList* dl, ImVec2 c, float sz, ImU32 col) {
    dl->AddTriangleFilled(ImVec2(c.x, c.y-sz*0.55f),
        ImVec2(c.x-sz*0.6f, c.y+sz*0.05f), ImVec2(c.x+sz*0.6f, c.y+sz*0.05f), col);
    dl->AddRectFilled(ImVec2(c.x-sz*0.4f, c.y+sz*0.05f),
        ImVec2(c.x+sz*0.4f, c.y+sz*0.6f), col, 2.f);
}
static void IconAccounts(ImDrawList* dl, ImVec2 c, float sz, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x, c.y-sz*0.15f), sz*0.32f, col);
    dl->AddCircleFilled(ImVec2(c.x, c.y+sz*0.55f), sz*0.55f, col);
}
static void IconVersions(ImDrawList* dl, ImVec2 c, float sz, ImU32 col) {
    dl->AddRect(ImVec2(c.x-sz*0.5f,c.y-sz*0.5f), ImVec2(c.x+sz*0.5f,c.y+sz*0.5f),
                col, 3.f, 0, 2.f);
    dl->AddLine(ImVec2(c.x-sz*0.3f,c.y-sz*0.2f), ImVec2(c.x+sz*0.3f,c.y-sz*0.2f), col, 2.f);
    dl->AddLine(ImVec2(c.x-sz*0.3f,c.y+sz*0.1f), ImVec2(c.x+sz*0.3f,c.y+sz*0.1f), col, 2.f);
    dl->AddLine(ImVec2(c.x-sz*0.3f,c.y+sz*0.3f), ImVec2(c.x+sz*0.1f,c.y+sz*0.3f), col, 2.f);
}
static void IconMods(ImDrawList* dl, ImVec2 c, float sz, ImU32 col) {
    dl->AddCircleFilled(ImVec2(c.x, c.y), sz*0.5f, col);
    dl->AddCircleFilled(ImVec2(c.x, c.y), sz*0.22f, COL_BG);
    for (int i = 0; i < 6; ++i) {
        float a = i * IM_PI / 3.f;
        ImVec2 p1(c.x+cosf(a)*sz*0.5f, c.y+sinf(a)*sz*0.5f);
        ImVec2 p2(c.x+cosf(a)*sz*0.7f, c.y+sinf(a)*sz*0.7f);
        dl->AddLine(p1, p2, col, 2.5f);
    }
}
static void IconSettings(ImDrawList* dl, ImVec2 c, float sz, ImU32 col) {
    dl->AddCircleFilled(c, sz*0.5f, col);
    dl->AddCircleFilled(c, sz*0.22f, COL_BG);
    for (int i = 0; i < 8; ++i) {
        float a = i * IM_PI / 4.f;
        ImVec2 p1(c.x+cosf(a)*sz*0.5f, c.y+sinf(a)*sz*0.5f);
        ImVec2 p2(c.x+cosf(a)*sz*0.72f, c.y+sinf(a)*sz*0.72f);
        dl->AddLine(p1, p2, col, 2.5f);
    }
}
static void IconExit(ImDrawList* dl, ImVec2 c, float sz, ImU32 col) {
    dl->AddRect(ImVec2(c.x-sz*0.5f,c.y-sz*0.5f), ImVec2(c.x+sz*0.4f,c.y+sz*0.5f),
                col, 3.f, 0, 2.f);
    dl->AddLine(ImVec2(c.x-sz*0.1f, c.y), ImVec2(c.x+sz*0.65f, c.y), col, 2.f);
    dl->AddTriangleFilled(ImVec2(c.x+sz*0.55f, c.y-sz*0.22f),
        ImVec2(c.x+sz*0.55f, c.y+sz*0.22f), ImVec2(c.x+sz*0.85f, c.y), col);
}

// ======================= SIDEBAR =======================
typedef void (*IconFn)(ImDrawList*, ImVec2, float, ImU32);

static bool SidebarButton(ImDrawList* dl, ImVec2 pos, float size, bool active,
                          IconFn icon, const char* id, const char* tooltip, int animIdx)
{
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(id, ImVec2(size, size));
    bool hov  = ImGui::IsItemHovered();
    bool clk  = ImGui::IsItemClicked();
    bool down = hov && ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // Smoothly fade the hover highlight in/out instead of an instant toggle.
    float& hoverAmt = g_sbHover[animIdx];
    hoverAmt = ExpSmooth(hoverAmt, hov ? 1.f : 0.f, 14.f);

    ImVec2 c(pos.x + size*0.5f, pos.y + size*0.5f);
    float r = size * 0.30f;

    // The active tab's background is drawn once as a sliding pill behind all
    // buttons (see DrawSidebar), so here we only draw the hover wash.
    if (!active && hoverAmt > 0.01f) {
        dl->AddRectFilled(pos, ImVec2(pos.x+size, pos.y+size),
                          IM_COL32(34,48,80,(int)(hoverAmt*190)), r);
    }

    // Small tactile "press" shrink for a springy, iOS-like feel.
    float iconScale = down ? 0.90f : 1.f;
    ImVec4 dimC  = ImGui::ColorConvertU32ToFloat4(COL_TEXT_DIM);
    ImVec4 hotC  = ImGui::ColorConvertU32ToFloat4(COL_TEXT);
    ImU32  colHov = ImGui::GetColorU32(ImLerp(dimC, hotC, hoverAmt));
    ImU32  col   = active ? U32(255,255,255) : colHov;
    icon(dl, c, size*0.42f*iconScale, col);

    if (hov && tooltip && *tooltip) {
        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f,1.f,1.f,1.f));
        ImGui::TextUnformatted(tooltip);
        ImGui::PopStyleColor();
        ImGui::EndTooltip();
    }
    return clk;
}

static void DrawSidebar(Launcher& L, ImDrawList* dl, ImVec2 pos, float w, float h) {
    // Soft drop shadow beneath the panel for a bit of iOS-style elevation.
    dl->AddRectFilled(ImVec2(pos.x+2, pos.y+5), ImVec2(pos.x+w+2, pos.y+h+5),
                      U32(0,0,0,55), 18.f);
    DrawGradientV(dl, pos, ImVec2(pos.x+w, pos.y+h), COL_PANEL, COL_PANEL_BOT, 18.f);

    float btn = 48.f;
    float x = pos.x + (w - btn) * 0.5f;
    float y0 = pos.y + 20.f;
    float gap = 10.f;

    // Sliding active-tab indicator: it eases towards whichever page is
    // selected instead of just popping into place.
    int pageIdx = (int)L.s().page; // Home,Accounts,Versions,Mods,Settings == 0..4
    float targetY = y0 + pageIdx * (btn + gap);
    if (g_sbIndicatorY < 0.f) g_sbIndicatorY = targetY;
    g_sbIndicatorY = ExpSmooth(g_sbIndicatorY, targetY, 16.f);

    float ir = btn * 0.30f;
    dl->AddRectFilled(ImVec2(x-4, g_sbIndicatorY-4), ImVec2(x+btn+4, g_sbIndicatorY+btn+4),
                      U32(64,140,245,55), ir+4);
    DrawGradientV(dl, ImVec2(x, g_sbIndicatorY), ImVec2(x+btn, g_sbIndicatorY+btn),
                  COL_ACCENT_HI, COL_ACCENT_LO, ir);

    float y = y0;
    if (SidebarButton(dl, ImVec2(x,y), btn, L.s().page == Page::Home,
                      IconHome, "##home", "Home", 0))
        L.s().page = Page::Home;
    y += btn + gap;

    if (SidebarButton(dl, ImVec2(x,y), btn, L.s().page == Page::Accounts,
                      IconAccounts, "##acc", "Accounts", 1))
        L.s().page = Page::Accounts;
    y += btn + gap;

    if (SidebarButton(dl, ImVec2(x,y), btn, L.s().page == Page::Versions,
                      IconVersions, "##vers", "Versions", 2))
        L.s().page = Page::Versions;
    y += btn + gap;

    if (SidebarButton(dl, ImVec2(x,y), btn, L.s().page == Page::Mods,
                      IconMods, "##mods", "Mods", 3))
        L.s().page = Page::Mods;
    y += btn + gap;

    if (SidebarButton(dl, ImVec2(x,y), btn, L.s().page == Page::Settings,
                      IconSettings, "##set", "Settings", 4))
        L.s().page = Page::Settings;

    // Discord
    {
        float dy = pos.y + h - btn - 20 - 40;
        ImVec2 dc(x + btn*0.5f, dy + btn*0.5f);
        bool ok = DiscordRPC::I().isReady();

        ImGui::SetCursorScreenPos(ImVec2(x, dy));
        ImGui::InvisibleButton("##dc", ImVec2(btn,btn));
        bool dcHov = ImGui::IsItemHovered();
        g_dcHover = ExpSmooth(g_dcHover, dcHov ? 1.f : 0.f, 14.f);

        if (g_dcHover > 0.01f)
            dl->AddCircleFilled(dc, btn*0.30f, IM_COL32(255,255,255,(int)(g_dcHover*22)));
        if (ok) {
            dl->AddCircleFilled(dc, btn*0.30f, U32(80,210,130,60));
            dl->AddCircleFilled(dc, btn*0.15f, COL_GREEN);
        } else {
            dl->AddCircleFilled(dc, btn*0.15f, U32(110,110,125));
        }
        if (dcHov) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ok ? ImVec4(0.31f,0.82f,0.51f,1.f)
                                  : ImVec4(0.6f,0.6f,0.65f,1.f),
                               ok ? "Discord: Connected" : "Discord: Not running");
            ImGui::EndTooltip();
        }
    }

    // Exit
    float exY = pos.y + h - btn - 20;
    if (SidebarButton(dl, ImVec2(x, exY), btn, false, IconExit, "##exit", "Exit", 5))
        PostQuitMessage(0);
}

// ======================= TOP BAR =======================
static void DrawTopBar(ImDrawList* dl, ImVec2 pos, float w, float h, const char* title) {
    dl->AddRectFilled(ImVec2(pos.x+2, pos.y+5), ImVec2(pos.x+w+2, pos.y+h+5),
                      U32(0,0,0,45), 18.f);
    DrawGradientV(dl, pos, ImVec2(pos.x+w, pos.y+h), COL_PANEL, COL_PANEL_BOT, 18.f);

    ensureLogo();
    float ls = 28.f;
    ImVec2 lp(pos.x + 18, pos.y + (h-ls)*0.5f);
    if (g_logoTex)
        dl->AddImage((ImTextureID)g_logoTex, lp, ImVec2(lp.x+ls, lp.y+ls));

    ImGui::PushFont(g_fontBold);
    ImVec2 tp(lp.x + ls + 12, pos.y + (h-18)*0.5f);
    dl->AddText(tp, COL_TEXT, title);
    ImGui::PopFont();
}

// ======================= AVATAR HELPER =======================
static void DrawAvatar(ImDrawList* dl, ImVec2 center, float radius,
                       const std::string& name, int colorIdx, bool ring)
{
    int r, g, b;
    AccountManager::avatarColor(colorIdx, r, g, b);

    // Nền tròn
    dl->AddCircleFilled(center, radius, U32(r, g, b));

    // Chữ cái đầu
    std::string initial = "P";
    if (!name.empty()) initial = std::string(1, toupper((unsigned char)name[0]));

    ImFont* f = g_fontBold ? g_fontBold : ImGui::GetFont();
    ImVec2 sz = f->CalcTextSizeA(radius*1.1f, FLT_MAX, 0.f, initial.c_str());

    ImGui::PushFont(f);
    dl->AddText(f, radius*1.1f,
        ImVec2(center.x - sz.x*0.5f, center.y - sz.y*0.5f),
        U32(255,255,255), initial.c_str());
    ImGui::PopFont();

    // Ring nếu active
    if (ring) {
        dl->AddCircle(center, radius+2, COL_GREEN, 0, 2.5f);
    }
}

// ======================= ACCOUNTS PAGE =======================
static void DrawAccountsPage(Launcher& L, ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawGradientV(dl, pos, ImVec2(pos.x+size.x, pos.y+size.y), COL_BG, COL_BG_BOT, 16.f);

    float pad = 24.f;

    ImGui::PushFont(g_fontBold);
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, pos.y+pad));
    ImGui::Text("Accounts");
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, pos.y+pad+34));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f,0.72f,0.88f,1.f));
    ImGui::Text("Add offline accounts and pick which one to launch with");
    ImGui::PopStyleColor();

    // Add new account
    float topY = pos.y + pad + 70;
    ImVec2 addCard(pos.x+pad, topY);
    ImVec2 addCardEnd(pos.x + size.x - pad, topY + 76);
    DrawGradientV(dl, addCard, addCardEnd, COL_CARD, COL_CARD_BOT, 14.f);

    ImGui::SetCursorScreenPos(ImVec2(addCard.x+18, addCard.y+16));
    ImGui::Text("New account");
    ImGui::SetCursorScreenPos(ImVec2(addCard.x+18, addCard.y+40));
    ImGui::SetNextItemWidth(280);
    ImGui::InputText("##newacc", g_newAccBuf, sizeof(g_newAccBuf), ImGuiInputTextFlags_CharsNoBlank);
    ImGui::SameLine();
    if (ImGui::Button("+ Add Account", ImVec2(150, 0))) {
        if (strlen(g_newAccBuf) > 0) {
            AccountManager::I().add(g_newAccBuf);
            g_newAccBuf[0] = 0;
        }
    }

    // List accounts
    float listY = topY + 90;
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, listY));
    ImGui::BeginChild("##acclist", ImVec2(size.x - pad*2, size.y - (listY - pos.y) - pad), false);

    auto& list = AccountManager::I().list();
    for (size_t i = 0; i < list.size(); ++i) {
        auto& a = list[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 84.f;

        // Nền card
        if (a.active) {
            // Gradient accent
            DrawGradientV(ImGui::GetWindowDrawList(), cp,
                          ImVec2(cp.x+cw, cp.y+ch),
                          U32(45, 75, 130), U32(30, 50, 90), 14.f);
        } else {
            DrawGradientV(ImGui::GetWindowDrawList(), cp,
                          ImVec2(cp.x+cw, cp.y+ch),
                          COL_CARD, COL_CARD_BOT, 14.f);
        }

        // Avatar
        ImVec2 avCenter(cp.x + 42, cp.y + ch*0.5f);
        DrawAvatar(ImGui::GetWindowDrawList(), avCenter, 24.f, a.name, a.colorIdx, a.active);

        // Name
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 84, cp.y + 16));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", a.name.c_str());
        ImGui::PopFont();

        // Status
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 84, cp.y + 42));
        if (a.active) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f,0.82f,0.51f,1.f));
            ImGui::Text("● Active");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f,0.72f,0.88f,1.f));
            ImGui::Text("○ Offline");
            ImGui::PopStyleColor();
        }

        // UUID nhỏ
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 84, cp.y + 60));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f,0.55f,0.75f,1.f));
        std::string uuidShort = a.uuid.substr(0, 16) + "...";
        ImGui::Text("%s", uuidShort.c_str());
        ImGui::PopStyleColor();

        // Buttons
        float bx = cp.x + cw - 340;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + 28));
        if (!a.active) {
            if (ImGui::Button("Set Active", ImVec2(100, 30)))
                AccountManager::I().setActive(i);
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("Current", ImVec2(100, 30));
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Rename", ImVec2(90, 30))) {
            // Đơn giản: đổi tên = thêm "2" vào cuối để demo
            // Trong bản đầy đủ sẽ mở popup nhập tên mới
            std::string nn = a.name + "_2";
            AccountManager::I().rename(i, nn);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(90, 30))) {
            AccountManager::I().remove(i);
            break;  // tránh lỗi iterator
        }

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + ch + 10));
    }

    ImGui::EndChild();
}

// ======================= HOME PAGE =======================
static void DrawHome(Launcher& L, ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawGradientV(dl, pos, ImVec2(pos.x+size.x, pos.y+size.y), COL_BG, COL_BG_BOT, 16.f);

    float pad = 32.f;

    // Title
    ImGui::PushFont(g_fontBig);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f,0.98f,1.f,1.f));
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y + pad));
    ImGui::Text("RavenXD");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y + pad + 42));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f,0.72f,0.88f,1.f));
    ImGui::Text("Minecraft 1.8.9  •  Forge  •  OptiFine");
    ImGui::PopStyleColor();

    // Profile card
    float cy = pos.y + pad + 92;
    ImVec2 c1(pos.x + pad, cy);
    ImVec2 c1e(pos.x + size.x - pad, cy + 84);
    DrawGradientV(dl, c1, c1e, COL_CARD, COL_CARD_BOT, 14.f);

    // Avatar hiện tại
    int activeIdx = AccountManager::I().activeIndex();
    std::string activeName = AccountManager::I().activeName();
    int colorIdx = 0;
    if (activeIdx >= 0) colorIdx = AccountManager::I().list()[activeIdx].colorIdx;
    DrawAvatar(dl, ImVec2(c1.x + 42, c1.y + 42), 28.f, activeName, colorIdx, true);

    ImGui::SetCursorScreenPos(ImVec2(c1.x + 84, c1.y + 16));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f,0.72f,0.88f,1.f));
    ImGui::Text("Profile");
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(ImVec2(c1.x + 84, c1.y + 38));
    ImGui::PushFont(g_fontBold);
    ImGui::Text("%s", activeName.c_str());
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(c1.x + 84, c1.y + 60));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f,0.55f,0.75f,1.f));
    ImGui::Text("Click Accounts in sidebar to switch");
    ImGui::PopStyleColor();

    // Version
    cy += 96;
    ImVec2 c2(pos.x + pad, cy);
    ImVec2 c2e(pos.x + size.x - pad, cy + 76);
    DrawGradientV(dl, c2, c2e, COL_CARD, COL_CARD_BOT, 14.f);

    ImGui::SetCursorScreenPos(ImVec2(c2.x + 18, c2.y + 14));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f,0.72f,0.88f,1.f));
    ImGui::Text("Version");
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(ImVec2(c2.x + 18, c2.y + 38));
    const char* versions[] = { "Minecraft 1.8.9", "Forge 1.8.9", "Forge 1.8.9 + OptiFine" };
    ImGui::SetNextItemWidth(380);
    ImGui::Combo("##ver", &L.s().selectedVersion, versions, 3);

    // LAUNCH
    cy += 104;
    float bw = 340, bh = 60;
    ImVec2 bp(pos.x + pad, cy);
    ImGui::SetCursorScreenPos(bp);
    bool busy = L.s().taskState == TaskState::Running;

    // Gentle breathing glow behind the CTA button when idle — draws the eye
    // to it the way an iOS "primary action" pulse would, without being
    // distracting once a task is actually running.
    g_launchGlowT += g_dt;
    float pulse = 0.5f + 0.5f * sinf(g_launchGlowT * 2.2f);
    int   glowA = busy ? 35 : (int)(40 + 28 * pulse);
    dl->AddRectFilled(ImVec2(bp.x-8, bp.y-8), ImVec2(bp.x+bw+8, bp.y+bh+8),
                     U32(64,140,245,glowA), 22.f);

    ImGui::PushFont(g_fontBold);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f,0.55f,0.96f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f,0.70f,1.f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f,0.45f,0.85f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.f);

    if (!busy) {
        if (ImGui::Button("     LAUNCH", ImVec2(bw, bh))) L.onLaunchClicked();
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("     WORKING...", ImVec2(bw, bh));
        ImGui::EndDisabled();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PopFont();

    // Progress
    cy += 88;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, cy));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.25f,0.55f,0.96f,1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.09f,0.13f,0.22f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    // Ease the displayed percentage towards the real value so fast download
    // ticks glide instead of jumping in steps.
    static float shownProgress = 0.f;
    shownProgress = ExpSmooth(shownProgress, L.s().progress, 8.f);
    float pct = shownProgress;
    char ov[64];
    if (L.s().speedMBps > 0.f)
        snprintf(ov, sizeof(ov), "%d%%   %.2f MB/s", (int)(pct*100.f), L.s().speedMBps);
    else
        snprintf(ov, sizeof(ov), "%d%%", (int)(pct*100.f));
    ImGui::ProgressBar(pct, ImVec2(size.x - pad*2, 22), ov);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    cy += 36;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, cy));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f,0.72f,0.88f,1.f));
    ImGui::Text("%s", L.s().statusText.c_str());
    ImGui::PopStyleColor();

    if (L.s().taskState == TaskState::Failed && !L.s().lastError.empty()) {
        cy += 24;
        ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, cy));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f,0.48f,0.48f,1.f));
        ImGui::TextWrapped("Error: %s", L.s().lastError.c_str());
        ImGui::PopStyleColor();
    }
}

// ======================= VERSIONS =======================
struct VerItem {
    const char* title;
    const char* sub;
    const char* tag;
    bool free;
    int mapSel;
    ImU32 c1, c2;
    ImU32 accent;
};

static VerItem g_versions[4] = {
    { "1.16.5",        "Nether",  "Client", false, 0,
      U32(180, 45, 90),  U32(255, 90, 60),  U32(255, 130, 90) },
    { "ALPHA 1.16.5",  "Aurora",  "Client", false, 1,
      U32(20, 130, 130), U32(60, 230, 190), U32(80, 240, 200) },
    { "LEGACY 1.12.2", "Classic", "Free",   true,  2,
      U32(60, 140, 230), U32(140, 200, 255),U32(120, 180, 255) },
    { "1.21.11",       "Forest",  "Client", false, 1,
      U32(50, 150, 70),  U32(150, 220, 110),U32(130, 210, 120) },
};

static bool VersionCard(int idx, ImVec2 pos, ImVec2 size, const VerItem& v, bool selected) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(v.title, size);
    bool hov = ImGui::IsItemHovered();
    bool clk = ImGui::IsItemClicked();

    // Smooth hover amount instead of an instant on/off, used to ease a
    // gentle "lift" (grow) and a fading glow border — an iOS-like tap feel.
    float& hoverAmt = g_verHover[idx];
    hoverAmt = ExpSmooth(hoverAmt, hov ? 1.f : 0.f, 12.f);

    float grow = hoverAmt * 5.f;
    ImVec2 gp0(pos.x - grow*0.5f, pos.y - grow*0.5f);
    ImVec2 gp1(pos.x + size.x + grow*0.5f, pos.y + size.y + grow*0.5f);

    float r = 14.f;
    DrawGradientV(dl, gp0, gp1, v.c1, v.c2, r);
    dl->AddRectFilled(gp0, gp1, U32(0,0,0, (int)(110 - 50*hoverAmt)), r);

    if (hoverAmt > 0.01f) {
        dl->AddRectFilled(ImVec2(gp0.x-2, gp0.y-2), ImVec2(gp1.x+2, gp1.y+2),
                          (v.accent & 0x00FFFFFF) | ((int)(0x40*hoverAmt) << 24), r+2);
    }

    if (selected)
        dl->AddRect(gp0, gp1, v.accent, r, 0, 3.f);
    else if (hoverAmt > 0.01f)
        dl->AddRect(gp0, gp1, IM_COL32(255,255,255,(int)(220*hoverAmt)), r, 0, 2.f);

    const char* tag = v.tag;
    ImVec2 ts = ImGui::CalcTextSize(tag);
    ImVec2 tpos(gp1.x - ts.x - 24, gp0.y + 12);
    ImU32 tbg = v.free ? U32(70,190,110) : U32(15,20,36,230);
    dl->AddRectFilled(tpos, ImVec2(tpos.x + ts.x + 20, tpos.y + ts.y + 10), tbg, 8.f);
    dl->AddText(ImVec2(tpos.x + 10, tpos.y + 5), U32(255,255,255), tag);

    ImGui::PushFont(g_fontBold);
    ImVec2 tt = ImGui::CalcTextSize(v.title);
    ImVec2 tp(gp0.x + 16, gp1.y - tt.y - 34);
    dl->AddText(ImVec2(tp.x+1, tp.y+1), U32(0,0,0,220), v.title);
    dl->AddText(tp, U32(255,255,255), v.title);
    ImGui::PopFont();

    ImVec2 st = ImGui::CalcTextSize(v.sub);
    dl->AddText(ImVec2(gp0.x + 16, gp1.y - st.y - 14),
                U32(230,235,245,220), v.sub);

    ImVec2 ac(gp1.x - 22, gp1.y - 22);
    ImU32 ac2 = U32(255,255,255);
    if (hoverAmt > 0.01f) dl->AddCircleFilled(ac, 15.f, IM_COL32(255,255,255,(int)(80*hoverAmt)));
    float al = 6.f;
    dl->AddLine(ImVec2(ac.x-al, ac.y), ImVec2(ac.x+al, ac.y), ac2, 2.f);
    dl->AddLine(ImVec2(ac.x+al, ac.y), ImVec2(ac.x+al*0.3f, ac.y-al*0.6f), ac2, 2.f);
    dl->AddLine(ImVec2(ac.x+al, ac.y), ImVec2(ac.x+al*0.3f, ac.y+al*0.6f), ac2, 2.f);

    return clk;
}

static void DrawVersionsPage(Launcher& L, ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawGradientV(dl, pos, ImVec2(pos.x+size.x, pos.y+size.y), COL_BG, COL_BG_BOT, 16.f);

    float pad = 24.f, gap = 16.f;
    int cols = 3;
    float cardW = (size.x - pad*2 - gap*(cols-1)) / cols;
    float cardH = 170.f;

    for (int i = 0; i < 4; ++i) {
        int row = i / cols, col = i % cols;
        ImVec2 cp(pos.x + pad + col*(cardW+gap), pos.y + pad + row*(cardH+gap));
        if (cp.x + cardW > pos.x + size.x - pad) continue;

        bool sel = (L.s().selectedVersion == g_versions[i].mapSel);
        if (VersionCard(i, cp, ImVec2(cardW, cardH), g_versions[i], sel)) {
            L.s().selectedVersion = g_versions[i].mapSel;
            Settings::I().d().version = g_versions[i].title;
            Settings::I().save();
            L.onLaunchClicked();
        }
    }
}

// ======================= MODS =======================
static void DrawMods(Launcher& L, ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawGradientV(dl, pos, ImVec2(pos.x+size.x, pos.y+size.y), COL_BG, COL_BG_BOT, 16.f);

    float pad = 24.f;
    ImGui::PushFont(g_fontBold);
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, pos.y+pad));
    ImGui::Text("Mods");
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 150, pos.y + pad + 4));
    if (ImGui::Button("Update All", ImVec2(120, 32)))
        L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y + pad + 50));
    ImGui::BeginChild("##modscroll", ImVec2(size.x - pad*2, size.y - pad*2 - 50), false);

    auto& mods = L.mods().mods();
    for (size_t i = 0; i < mods.size(); ++i) {
        auto& m = mods[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = 92.f;

        DrawGradientV(ImGui::GetWindowDrawList(), cp,
                      ImVec2(cp.x+cw, cp.y+ch), COL_CARD, COL_CARD_BOT, 14.f);

        ImGui::SetCursorScreenPos(ImVec2(cp.x+18, cp.y+14));
        ImGui::PushFont(g_fontBold);
        ImGui::Text("%s", m.name.c_str());
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(cp.x+18, cp.y+40));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f,0.72f,0.88f,1.f));
        ImGui::Text("v%s  •  %s  •  %s", m.version.c_str(), m.source.c_str(),
                    m.installed ? "installed" : "not installed");
        ImGui::PopStyleColor();

        float bx = cp.x + cw - 320;
        ImGui::SetCursorScreenPos(ImVec2(bx, cp.y + 32));
        ImGui::PushStyleColor(ImGuiCol_Text,
            m.enabled ? ImVec4(0.35f,0.82f,0.51f,1.f) : ImVec4(0.85f,0.55f,0.55f,1.f));
        ImGui::Text("%s", m.enabled ? "ON" : "OFF");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button(m.enabled ? "Disable" : "Enable", ImVec2(80, 30)))
            L.mods().setEnabled(i, !m.enabled, Settings::I().d().minecraftDir);
        ImGui::SameLine();
        if (ImGui::Button("Update", ImVec2(80, 30)))
            L.mods().updateAll(Settings::I().d().minecraftDir, nullptr);
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(80, 30)))
            L.mods().remove(i, Settings::I().d().minecraftDir);

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y+ch+10));
    }
    ImGui::EndChild();
}

// ======================= iOS TOGGLE SWITCH =======================
static bool IOSToggle(int animIdx, const char* id, bool value) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = 46.f, h = 26.f;

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();
    bool hov = ImGui::IsItemHovered();

    float& anim = g_toggleAnim[animIdx];
    anim = ExpSmooth(anim, value ? 1.f : 0.f, 16.f);

    ImVec4 offC = ImVec4(0.27f, 0.31f, 0.39f, 1.f);
    ImVec4 onC  = ImGui::ColorConvertU32ToFloat4(COL_GREEN);
    ImU32 track = ImGui::GetColorU32(ImLerp(offC, onC, anim));
    if (hov) track = ImGui::GetColorU32(ImLerp(ImGui::ColorConvertU32ToFloat4(track),
                                               ImVec4(1,1,1,1), 0.08f));

    float r = h * 0.5f;
    dl->AddRectFilled(pos, ImVec2(pos.x+w, pos.y+h), track, r);

    float knobR = r - 3.f;
    float knobX = pos.x + r + anim * (w - h);
    float knobY = pos.y + r;
    dl->AddCircleFilled(ImVec2(knobX, knobY+1.5f), knobR+1.5f, U32(0,0,0,70)); // soft shadow
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, U32(255,255,255));

    return clicked;
}

// ======================= SETTINGS =======================
static void DrawSettings(Launcher& L, ImVec2 pos, ImVec2 size) {
    (void)L;
    auto& d = Settings::I().d();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawGradientV(dl, pos, ImVec2(pos.x+size.x, pos.y+size.y), COL_BG, COL_BG_BOT, 16.f);

    float pad = 24.f;
    ImGui::PushFont(g_fontBold);
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad, pos.y+pad));
    ImGui::Text("Settings");
    ImGui::PopFont();

    float cy = pos.y + pad + 50;
    float cw = size.x - pad*2;

    auto Card = [&](float h){
        ImVec2 cp(pos.x+pad, cy);
        DrawGradientV(dl, cp, ImVec2(cp.x+cw, cp.y+h), COL_CARD, COL_CARD_BOT, 14.f);
        ImGui::SetCursorScreenPos(ImVec2(cp.x+18, cp.y+16));
    };

    Card(88);
    ImGui::Text("Minecraft Directory");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+48));
    static char dirB[512];
    strncpy_s(dirB, d.minecraftDir.c_str(), sizeof(dirB)-1);
    ImGui::SetNextItemWidth(cw - 36);
    if (ImGui::InputText("##dir", dirB, sizeof(dirB))) {
        d.minecraftDir = dirB; Settings::I().save();
    }
    cy += 96;

    Card(88);
    ImGui::Text("Java Path");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+48));
    static char jB[512];
    strncpy_s(jB, d.javaPath.c_str(), sizeof(jB)-1);
    ImGui::SetNextItemWidth(cw - 200);
    if (ImGui::InputText("##java", jB, sizeof(jB))) {
        d.javaPath = jB; Settings::I().save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto Detect", ImVec2(140, 0))) {
        d.javaPath = Minecraft::FindJava();
        Settings::I().save();
        strncpy_s(jB, d.javaPath.c_str(), sizeof(jB)-1);
    }
    cy += 96;

    Card(136);
    ImGui::Text("RAM (MB)");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+48));
    ImGui::SetNextItemWidth(240);
    if (ImGui::SliderInt("##ram", &d.ramMB, 512, 16384)) Settings::I().save();

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+86));
    ImGui::Text("Window");
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+95, cy+86));
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##ww", &d.windowWidth, 0, 0)) Settings::I().save();
    ImGui::SameLine(); ImGui::Text("x"); ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##wh", &d.windowHeight, 0, 0)) Settings::I().save();
    cy += 144;

    Card(130);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Close launcher after launch");
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad + cw - 18 - 46, cy + 11));
    if (IOSToggle(0, "##toggle_close", d.closeAfterLaunch)) {
        d.closeAfterLaunch = !d.closeAfterLaunch;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+52));
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Debug Mode");
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad + cw - 18 - 46, cy + 47));
    if (IOSToggle(1, "##toggle_debug", d.debugMode)) {
        d.debugMode = !d.debugMode;
        Settings::I().save();
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+18, cy+88));
    if (ImGui::Button("Open RavenXD Folder", ImVec2(200, 28))) {
        std::string p = Settings::I().appDataDir();
        ShellExecuteA(nullptr, "open", p.c_str(), nullptr, nullptr, SW_SHOW);
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x+pad+245, cy+92));
    bool dc = DiscordRPC::I().isReady();
    ImGui::TextColored(dc ? ImVec4(0.35f,0.82f,0.51f,1.f)
                          : ImVec4(0.6f,0.6f,0.65f,1.f),
                       dc ? "Discord: Connected" : "Discord: Not running");
}

// ======================= JAVA POPUP =======================
static void DrawJavaPopup(Launcher& L) {
    if (L.s().showJavaPopup) ImGui::OpenPopup("Java");
    ImGui::SetNextWindowSize(ImVec2(430, 0));
    if (ImGui::BeginPopupModal("Java", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(g_fontBold);
        ImGui::TextColored(ImVec4(1.f,0.55f,0.55f,1.f), "Java not found");
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
        if (ImGui::Button("Get Java 8", ImVec2(120, 36))) {
            ShellExecuteA(nullptr, "open",
                "https://adoptium.net/temurin/releases/?version=8",
                nullptr, nullptr, SW_SHOW);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80, 36))) {
            L.s().showJavaPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ======================= LOADING =======================
void UI::RenderLoadingScreen(Launcher& L) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, COL_BG);

    float t = L.s().loadingTimer;
    float a = t < 0.6f ? t/0.6f : 1.f;
    float sc = 0.85f + 0.15f * (t > 1.0f ? 1.f : t);
    if (sc > 1.f) sc = 1.f;

    float cx = io.DisplaySize.x * 0.5f;
    float cy = io.DisplaySize.y * 0.5f - 30;
    float sz = 120.f * sc;

    ensureLogo();
    if (g_logoTex) {
        dl->AddImage((ImTextureID)g_logoTex,
                     ImVec2(cx-sz*0.5f, cy-sz*0.5f),
                     ImVec2(cx+sz*0.5f, cy+sz*0.5f),
                     ImVec2(0,0), ImVec2(1,1),
                     IM_COL32(255,255,255,(int)(a*255)));
    } else {
        dl->AddText(ImVec2(cx-40, cy-10), IM_COL32(123,167,224,(int)(a*255)), "RavenXD");
    }

    const char* ph = t < 0.8f ? "Checking files..." :
                     t < 1.6f ? "Loading launcher..." : "Ready";
    ImVec2 ts = ImGui::CalcTextSize(ph);
    dl->AddText(ImVec2(cx-ts.x*0.5f, cy+90),
                IM_COL32(170,185,210,(int)(a*255)), ph);
}

// ======================= ROOT =======================
void UI::Render(Launcher& L, float dt) {
    g_dt = dt > 0.f ? dt : (1.f/60.f);
    L.tick(dt);

    if (L.s().showLoadingScreen) {
        RenderLoadingScreen(L);
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background — gradient hoặc ảnh
    ensureBg();
    if (g_bgTex) {
        // Ảnh nền full screen
        dl->AddImage((ImTextureID)g_bgTex, ImVec2(0,0), io.DisplaySize,
                     ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255));
        // Overlay tối để UI nổi
        dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, U32(4, 8, 16, 200));
    } else {
        // Fallback gradient
        DrawGradientV(dl, ImVec2(0,0), io.DisplaySize, COL_BG, COL_BG_BOT, 0.f);
    }

    float sidebarW = 72.f;
    float topH     = 56.f;
    float margin   = 12.f;

    DrawSidebar(L, dl, ImVec2(margin, margin + topH + 8),
                sidebarW, io.DisplaySize.y - margin*2 - topH - 8);

    const char* title = "RavenXD";
    switch (L.s().page) {
        case Page::Home:     title = "RavenXD";    break;
        case Page::Accounts: title = "Accounts";   break;
        case Page::Versions: title = "Versions";   break;
        case Page::Mods:     title = "Mods";       break;
        case Page::Settings: title = "Settings";   break;
    }
    DrawTopBar(dl, ImVec2(margin + sidebarW + 8, margin),
               io.DisplaySize.x - margin*2 - sidebarW - 8, topH, title);

    ImVec2 cpos(margin + sidebarW + 8, margin + topH + 8);
    ImVec2 csz(io.DisplaySize.x - margin*2 - sidebarW - 8,
               io.DisplaySize.y - margin*2 - topH - 8);

    ImGui::SetNextWindowPos(cpos);
    ImGui::SetNextWindowSize(csz);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##content", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground);

    ImGui::PushFont(g_fontRegular);

    switch (L.s().page) {
        case Page::Home:     DrawHome(L, cpos, csz); break;
        case Page::Accounts: DrawAccountsPage(L, cpos, csz); break;
        case Page::Versions: DrawVersionsPage(L, cpos, csz); break;
        case Page::Mods:     DrawMods(L, cpos, csz); break;
        case Page::Settings: DrawSettings(L, cpos, csz); break;
    }

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();

    DrawJavaPopup(L);
}
