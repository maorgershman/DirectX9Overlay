#pragma once

#include <string>
#include <functional>

#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>

struct DirectX9Overlay
{
    static constexpr auto dxDevice_vTable_funcCount = 119;
    static constexpr auto dxDevice_vTable_EndSceneFunc_index = 42;
    static constexpr auto dxDevice_vTable_EndSceneFunc_nStolenBytes =
        (sizeof(uint8_t) + sizeof(uint8_t)) + // push    14                   ---> 6A 14
        (sizeof(uint8_t) + sizeof(void*));    // mov     eax, d3d9.dll+8012E  ---> B8 2E01E96B

    static HWND hWnd;

    DirectX9Overlay(std::string strWindowName, std::function<void(PDIRECT3DDEVICE9)> pRenderFunc, std::function<void()> pCleanupFunc) noexcept;
    ~DirectX9Overlay();

private:
    void* dxDevice_vTable[dxDevice_vTable_funcCount]{};

    bool init_dxDevice_vTable() noexcept;
};