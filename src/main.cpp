#include "UI/UI.h"
#include "Launcher.h"
#include "Settings.h"
#include "DiscordRPC.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <windows.h>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

static ID3D11Device*           g_pd3dDevice        = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain        = nullptr;
static UINT                    g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

ImFont* g_fontRegular = nullptr;
ImFont* g_fontBold    = nullptr;
ImFont* g_fontBig     = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);

// ============================================================
//  Texture loader (stb_image)
// ============================================================
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void LoadTextureFromMemory(const unsigned char* data, size_t len,
                           ID3D11ShaderResourceView** out)
{
    int w, h, ch;
    unsigned char* pixels = stbi_load_from_memory(data, (int)len, &w, &h, &ch, 4);
    if (!pixels) return;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w; desc.Height = h;
    desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub = {};
    sub.pSysMem = pixels;
    sub.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &sub, &tex))) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = desc.Format;
        srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        g_pd3dDevice->CreateShaderResourceView(tex, &srv, out);
        tex->Release();
    }
    stbi_image_free(pixels);
}

// ============================================================
//  FONT LOADING — SF Pro Display + fallback
// ============================================================
static bool FileExistsA(const char* p) {
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}

static void LoadFonts(ImGuiIO& io)
{
    ImFontConfig fc;
    fc.OversampleH        = 2;
    fc.OversampleV        = 2;
    fc.PixelSnapH         = false;
    fc.RasterizerMultiply = 1.05f;

    // ---- SF Pro Display paths (ưu tiên) ----
    const char* reg[]  = { "fonts/SFProDisplay-Regular.ttf",
                           "assets/fonts/SFProDisplay-Regular.ttf" };
    const char* bold[] = { "fonts/SFProDisplay-Bold.ttf",
                           "assets/fonts/SFProDisplay-Bold.ttf" };

    g_fontRegular = nullptr;
    g_fontBold    = nullptr;
    g_fontBig     = nullptr;

    // Regular 15
    for (auto* p : reg)
        if (FileExistsA(p)) { g_fontRegular = io.Fonts->AddFontFromFileTTF(p, 15.f, &fc); break; }

    // Bold 15
    for (auto* p : bold)
        if (FileExistsA(p)) { g_fontBold = io.Fonts->AddFontFromFileTTF(p, 15.f, &fc); break; }

    // Big 26 (bold)
    for (auto* p : bold)
        if (FileExistsA(p)) { g_fontBig = io.Fonts->AddFontFromFileTTF(p, 26.f, &fc); break; }

    // ---- Fallback: Segoe UI ----
    if (!g_fontRegular && FileExistsA("C:\\Windows\\Fonts\\segoeui.ttf"))
        g_fontRegular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.f, &fc);
    if (!g_fontBold && FileExistsA("C:\\Windows\\Fonts\\segoeuib.ttf"))
        g_fontBold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 15.f, &fc);
    if (!g_fontBig && FileExistsA("C:\\Windows\\Fonts\\segoeuib.ttf"))
        g_fontBig = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 26.f, &fc);

    // ---- Last resort: ImGui default ----
    if (!g_fontRegular) g_fontRegular = io.Fonts->AddFontDefault();
    if (!g_fontBold)    g_fontBold    = g_fontRegular;
    if (!g_fontBig)     g_fontBig     = g_fontBold;
}

// ============================================================
//  WinMain
// ============================================================
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"RavenXD";
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"RavenXD",
        WS_OVERLAPPEDWINDOW, 100, 100, 1100, 720,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    LoadFonts(io);
    UI::ApplyStyle();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    Settings::I().load();
    DiscordRPC::I().init("1234567890");

    Launcher launcher;

    bool running = true;
    auto last = std::chrono::steady_clock::now();

    while (running) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight,
                                        DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        UI::Render(launcher, dt);

        ImGui::Render();

        const float clear[4] = { 0.06f, 0.09f, 0.15f, 1.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &got, &g_pd3dDeviceContext);

    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
            levels, 2, D3D11_SDK_VERSION,
            &sd, &g_pSwapChain, &g_pd3dDevice, &got, &g_pd3dDeviceContext);
    }
    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRenderTargetView);
        back->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth  = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
