#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <vector>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Launcher.h"
#include "UI/UI.h"
#include "DiscordRPC.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

ID3D11Device*            g_pd3dDevice        = nullptr;
ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*   g_pSwapChain        = nullptr;
static ID3D11RenderTargetView* g_mainRTV     = nullptr;
static UINT              g_ResizeW = 0, g_ResizeH = 0;
static HWND              g_hWnd = nullptr;

// ImGui fonts (global — UI.cpp sẽ dùng)
ImFont* g_fontRegular = nullptr;
ImFont* g_fontBold    = nullptr;
ImFont* g_fontBig     = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ============ FONT LOADER ============
static std::string FindFontFile(const char* name) {
    char winDir[MAX_PATH];
    GetWindowsDirectoryA(winDir, MAX_PATH);
    std::string p = std::string(winDir) + "\\Fonts\\" + name;
    if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;

    // User fonts
    char appData[MAX_PATH];
    if (GetEnvironmentVariableA("LOCALAPPDATA", appData, MAX_PATH)) {
        std::string p2 = std::string(appData) + "\\Microsoft\\Windows\\Fonts\\" + name;
        if (GetFileAttributesA(p2.c_str()) != INVALID_FILE_ATTRIBUTES) return p2;
    }
    return "";
}

static void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // Cố gắng load SF Pro Display (nếu có) → fallback Segoe UI
    std::string regularPaths[] = {
        FindFontFile("SF-Pro-Display-Regular.otf"),
        FindFontFile("SFProDisplay-Regular.otf"),
        FindFontFile("segoeui.ttf"),
    };
    std::string boldPaths[] = {
        FindFontFile("SF-Pro-Display-Bold.otf"),
        FindFontFile("SFProDisplay-Bold.otf"),
        FindFontFile("segoeuib.ttf"),
    };
    std::string blackPaths[] = {
        FindFontFile("SF-Pro-Display-Black.otf"),
        FindFontFile("SFProDisplay-Black.otf"),
        FindFontFile("segoeuib.ttf"),
    };

    // Load regular
    for (auto& p : regularPaths) {
        if (!p.empty() && GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
            g_fontRegular = io.Fonts->AddFontFromFileTTF(p.c_str(), 16.0f);
            if (g_fontRegular) break;
        }
    }
    // Load bold
    for (auto& p : boldPaths) {
        if (!p.empty() && GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
            g_fontBold = io.Fonts->AddFontFromFileTTF(p.c_str(), 17.0f);
            if (g_fontBold) break;
        }
    }
    // Load big (title)
    for (auto& p : blackPaths) {
        if (!p.empty() && GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
            g_fontBig = io.Fonts->AddFontFromFileTTF(p.c_str(), 26.0f);
            if (g_fontBig) break;
        }
    }

    // Fallback nếu fail hết
    if (!g_fontRegular) g_fontRegular = io.Fonts->AddFontDefault();
    if (!g_fontBold)    g_fontBold    = g_fontRegular;
    if (!g_fontBig)     g_fontBig     = g_fontBold;
}

// ============ D3D11 ============
static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL lvl;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
        &lvl, &g_pd3dDeviceContext);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            0, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
            &lvl, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* pBack = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBack));
    if (pBack) {
        g_pd3dDevice->CreateRenderTargetView(pBack, nullptr, &g_mainRTV);
        pBack->Release();
    }
    return true;
}

static void CleanupD3D() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRTV() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
    ID3D11Texture2D* pBack = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBack));
    if (pBack) {
        g_pd3dDevice->CreateRenderTargetView(pBack, nullptr, &g_mainRTV);
        pBack->Release();
    }
}

// ============ TEXTURE LOADER (WIC) ============
void LoadTextureFromMemory(const unsigned char* data, size_t len,
                           ID3D11ShaderResourceView** out)
{
    if (!data || len == 0 || !out) return;

    IWICImagingFactory* factory = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&factory));
    if (!factory) return;

    IWICStream* stream = nullptr;
    factory->CreateStream(&stream);
    stream->InitializeFromMemory((BYTE*)data, (DWORD)len);

    IWICBitmapDecoder* decoder = nullptr;
    factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (!decoder) { stream->Release(); factory->Release(); return; }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);

    IWICFormatConverter* conv = nullptr;
    factory->CreateFormatConverter(&conv);
    conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                     WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    std::vector<BYTE> pixels(w * h * 4);
    conv->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub{};
    sub.pSysMem = pixels.data();
    sub.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    g_pd3dDevice->CreateTexture2D(&desc, &sub, &tex);
    if (tex) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, out);
        tex->Release();
    }

    conv->Release(); frame->Release(); decoder->Release();
    stream->Release(); factory->Release();
}

// ============ WINDOW PROC ============
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                g_ResizeW = LOWORD(lParam);
                g_ResizeH = HIWORD(lParam);
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void EnableRoundedCorners(HWND hWnd) {
    DWORD pref = 2;
    DwmSetWindowAttribute(hWnd, 33, &pref, sizeof(pref));
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hWnd, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hWnd, 19, &dark, sizeof(dark));
}

// ============ WINMAIN ============
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"RavenXDClass";
    RegisterClassExW(&wc);

    int W = 900, H = 560;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;

    g_hWnd = CreateWindowExW(
        0, wc.lpszClassName, L"RavenXD",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        sx, sy, W, H,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!g_hWnd) return 1;
    EnableRoundedCorners(g_hWnd);

    if (!CreateDeviceD3D(g_hWnd)) {
        CleanupD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    LoadFonts();
    UI::ApplyStyle();

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    Launcher launcher;
    launcher.init();

    DiscordRPC::I().init();
    DiscordRPC::I().setIdle();

    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        if (g_ResizeW != 0 && g_ResizeH != 0) {
            g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
            if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
            g_pSwapChain->ResizeBuffers(0, g_ResizeW, g_ResizeH,
                                        DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeW = g_ResizeH = 0;
            CreateRTV();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = float(now.QuadPart - last.QuadPart) / float(freq.QuadPart);
        last = now;

        UI::Render(launcher, dt);

        ImGui::Render();
        const float clear[4] = { 0.03f, 0.05f, 0.10f, 1.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRTV, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    DiscordRPC::I().shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupD3D();
    DestroyWindow(g_hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    return 0;
}
