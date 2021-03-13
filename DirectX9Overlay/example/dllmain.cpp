/*
    Example of drawing 'Hello World' in CSGO
*/

#include "../directx9_overlay.hpp"
#include <thread>

static void on_dll_attach();

BOOL WINAPI DllMain(HINSTANCE, DWORD dwReason, PVOID)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        std::thread thread(on_dll_attach);
        thread.detach();
    }
    return TRUE;
}

class Graphics
{
    static inline LPD3DXFONT pDxFontDefault;

public:
    /*
        Draw 'Hello World' in the middle left side of the screen in green color
    */
    static void render(PDIRECT3DDEVICE9 pDxDevice)
    {
        if (!pDxFontDefault)
        {
            D3DXCreateFont(pDxDevice, 24, 0, FW_BOLD, 1, false, DEFAULT_CHARSET,
                OUT_DEVICE_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Comic Sans MS", &pDxFontDefault);
        }

        const auto draw_string = [&](std::string text, int x, int y, D3DCOLOR dxColor, LPD3DXFONT pDxFont) {
            const auto len = text.length();

            RECT rect{ x, y };
            pDxFont->DrawTextA(NULL, text.c_str(), len, &rect, DT_NOCLIP, dxColor);
        };

        RECT rect{};
        GetClientRect(DirectX9Overlay::hWnd, &rect);

        draw_string("Hello World!", 15, (rect.bottom - rect.top) / 2, D3DCOLOR_ARGB(255, 0, 255, 0), pDxFontDefault);
    }

    static void cleanup()
    {
        if (pDxFontDefault)
        {
            pDxFontDefault->Release();
        }
    }
};

static void on_dll_attach()
{
    DirectX9Overlay d3d9Hook
    (
        "Counter-Strike: Global Offensive",
        Graphics::render,
        Graphics::cleanup
    );

    // Once insert key is pressed, the dll will deload
    while (!GetAsyncKeyState(VK_INSERT))
    {
        Sleep(100);
    }
}