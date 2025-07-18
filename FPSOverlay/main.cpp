// fps_overlay.cpp
// GPU-gyors�tott FPS overlay Win32 + Direct3D11 + Direct2D + DirectWrite

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <chrono>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")   
#pragma comment(lib, "dxguid.lib")          
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

HWND g_hWnd = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

ID2D1Factory1* g_pD2DFactory = nullptr;
ID2D1Device* g_pD2DDevice = nullptr;
ID2D1DeviceContext* g_pD2DContext = nullptr;
ID2D1Bitmap1* g_pD2DTargetBitmap = nullptr;

IDWriteFactory* g_pDWriteFactory = nullptr;
IDWriteTextFormat* g_pTextFormat = nullptr;
ID2D1SolidColorBrush* g_pBrush = nullptr;


std::chrono::steady_clock::time_point g_lastTime;
int   g_frameCount = 0;
float g_fps = 0.0f;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitWindow(HINSTANCE hInst, int nCmdShow);
HRESULT InitD3D();
HRESULT InitD2D();
HRESULT InitDWrite();
void    Cleanup();
void    RenderFrame();


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    if (FAILED(InitWindow(hInstance, nCmdShow))) return -1;
    if (FAILED(InitD3D()))      return -1;
    if (FAILED(InitD2D()))      return -1;
    if (FAILED(InitDWrite()))   return -1;


    g_lastTime = std::chrono::steady_clock::now();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            RenderFrame();
        }
    }

    Cleanup();
    return 0;
}


HRESULT InitWindow(HINSTANCE hInst, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"FPSOverlayClass";

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0,
                       hInst, nullptr, nullptr, nullptr, nullptr,
                       CLASS_NAME, nullptr };
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        CLASS_NAME, L"FPS Overlay",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr
    );
    if (!g_hWnd) return E_FAIL;

    SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return S_OK;
}

HRESULT InitD3D() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = GetSystemMetrics(SM_CXSCREEN);
    sd.BufferDesc.Height = GetSystemMetrics(SM_CYSCREEN);
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, nullptr, 0,
        D3D11_SDK_VERSION,
        &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel,
        &g_pImmediateContext
    );
    if (FAILED(hr)) return hr;

    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr)) return hr;
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) return hr;
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = (FLOAT)GetSystemMetrics(SM_CXSCREEN);
    vp.Height = (FLOAT)GetSystemMetrics(SM_CYSCREEN);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_pImmediateContext->RSSetViewports(1, &vp);

    return S_OK;
}

HRESULT InitD2D() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
    if (FAILED(hr)) return hr;

    IDXGIDevice* dxgiDevice = nullptr;
    hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr)) return hr;
    hr = g_pD2DFactory->CreateDevice(dxgiDevice, &g_pD2DDevice);
    dxgiDevice->Release();
    if (FAILED(hr)) return hr;

    hr = g_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_pD2DContext);
    if (FAILED(hr)) return hr;

    IDXGISurface* dxgiBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&dxgiBackBuffer);
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    hr = g_pD2DContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer, &props, &g_pD2DTargetBitmap);
    dxgiBackBuffer->Release();
    if (FAILED(hr)) return hr;

    g_pD2DContext->SetTarget(g_pD2DTargetBitmap);
    return S_OK;
}

HRESULT InitDWrite() {
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        (IUnknown**)&g_pDWriteFactory
    );
    if (FAILED(hr)) return hr;

    hr = g_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        24.0f, L"",
        &g_pTextFormat
    );
    if (FAILED(hr)) return hr;

    hr = g_pD2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_pBrush);
    return hr;
}

void RenderFrame() {
    g_frameCount++;
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - g_lastTime).count();
    if (elapsed >= 1.0f) {
        g_fps = g_frameCount / elapsed;
        g_frameCount = 0;
        g_lastTime = now;
    }

    const FLOAT clearColor[4] = { 0, 0, 0, 0 };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

    g_pD2DContext->BeginDraw();
    std::wstring text = L"FPS: " + std::to_wstring((int)g_fps);
    D2D1_SIZE_F rtSize = g_pD2DContext->GetSize();
    D2D1_RECT_F layout = D2D1::RectF(
        rtSize.width - 150.0f, 10.0f,
        rtSize.width - 10.0f, 50.0f
    );
    g_pD2DContext->DrawText(
        text.c_str(), (UINT32)text.length(),
        g_pTextFormat, layout, g_pBrush,
        D2D1_DRAW_TEXT_OPTIONS_NONE,
        DWRITE_MEASURING_MODE_NATURAL
    );
    g_pD2DContext->EndDraw();

    g_pSwapChain->Present(0, 0);
}

// Takar�t�s
void Cleanup() {
    if (g_pBrush)            g_pBrush->Release();
    if (g_pTextFormat)       g_pTextFormat->Release();
    if (g_pDWriteFactory)    g_pDWriteFactory->Release();
    if (g_pD2DTargetBitmap)  g_pD2DTargetBitmap->Release();
    if (g_pD2DContext)       g_pD2DContext->Release();
    if (g_pD2DDevice)        g_pD2DDevice->Release();
    if (g_pD2DFactory)       g_pD2DFactory->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice)        g_pd3dDevice->Release();
    if (g_pSwapChain)        g_pSwapChain->Release();
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
        UnregisterClassW(L"FPSOverlayClass", GetModuleHandle(nullptr));
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}
