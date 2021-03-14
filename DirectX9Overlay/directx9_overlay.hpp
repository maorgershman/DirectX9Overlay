#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <functional>

#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>

class DirectX9Overlay
{
    class Detour
    {
        static constexpr uint8_t JMP = 0xE9;
        static constexpr uint8_t NOP = 0x90;

    public:
        template<typename Func_t, size_t nStolenBytes>
        static Func_t hook(void* const pFunc_vTable, void* const pFunc_toHook, std::array<uint8_t, nStolenBytes>& arrStolenBytes) noexcept
        {
            static_assert(nStolenBytes >= sizeof(JMP) + sizeof(void*));

            // Copy the stolen bytes from the original function to the buffer
            memcpy(arrStolenBytes.data(), pFunc_vTable, nStolenBytes);

            /////////////////////////
            // Prepare the gateway //
            /////////////////////////

            void* pFunc_gateway = VirtualAlloc(nullptr, nStolenBytes, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            // Copy the stolen bytes from the vTable function to the gateway
            memcpy(pFunc_gateway, pFunc_vTable, nStolenBytes);

            const auto gatewayRelativeAddress =
                reinterpret_cast<ptrdiff_t>(pFunc_vTable) -
                reinterpret_cast<ptrdiff_t>(pFunc_gateway) -
                (sizeof(JMP) + sizeof(void*));

            memcpy(static_cast<uint8_t*>(pFunc_gateway) + nStolenBytes, &JMP, sizeof(JMP));
            memcpy(static_cast<uint8_t*>(pFunc_gateway) + nStolenBytes + sizeof(JMP), &gatewayRelativeAddress, sizeof(gatewayRelativeAddress));

            //////////
            // Hook //
            //////////

            DWORD dwProtection;
            VirtualProtect(pFunc_vTable, nStolenBytes, PAGE_EXECUTE_READWRITE, &dwProtection);

            // "Zero" the memory with NOPs
            memset(pFunc_vTable, NOP, nStolenBytes);

            const auto relativeAddress =
                reinterpret_cast<ptrdiff_t>(pFunc_toHook) -
                reinterpret_cast<ptrdiff_t>(pFunc_vTable) -
                (sizeof(JMP) + sizeof(void*));

            memcpy(pFunc_vTable, &JMP, sizeof(JMP));
            memcpy(static_cast<uint8_t*>(pFunc_vTable) + sizeof(JMP), &relativeAddress, sizeof(relativeAddress));

            DWORD dwTemp;
            VirtualProtect(pFunc_vTable, nStolenBytes, dwProtection, &dwTemp);

            return reinterpret_cast<Func_t>(pFunc_gateway);
        }

        template<size_t nStolenBytes>
        static void unhook(void* const pFunc_vTable, std::array<uint8_t, nStolenBytes>& arrStolenBytes) noexcept
        {
            static_assert(nStolenBytes >= sizeof(JMP) + sizeof(void*));

            DWORD dwProtection;
            VirtualProtect(pFunc_vTable, nStolenBytes, PAGE_EXECUTE_READWRITE, &dwProtection);

            // Restore the original bytes that were stolen
            memcpy(static_cast<uint8_t*>(pFunc_vTable), arrStolenBytes.data(), nStolenBytes);

            DWORD dwTemp;
            VirtualProtect(pFunc_vTable, nStolenBytes, dwProtection, &dwTemp);
        }

    };

    typedef HRESULT(__stdcall* dxDevice_EndSceneFunc_t)(PDIRECT3DDEVICE9);

    static constexpr auto dxDevice_vTable_funcCount = 119;
    static constexpr auto dxDevice_vTable_EndSceneFunc_index = 42;
    static constexpr auto dxDevice_vTable_EndSceneFunc_nStolenBytes =
        (sizeof(uint8_t) + sizeof(uint8_t)) + // push    14                   ---> 6A 14
        (sizeof(uint8_t) + sizeof(void*));    // mov     eax, d3d9.dll+8012E  ---> B8 2E01E96B

    static inline std::function<void(PDIRECT3DDEVICE9 pDxDevice)> pRenderFunc;
    static inline std::function<void()> pCleanupFunc;

    static inline dxDevice_EndSceneFunc_t pDxDevice_EndSceneFunc_org;
    static inline std::array<uint8_t, DirectX9Overlay::dxDevice_vTable_EndSceneFunc_nStolenBytes> dxDevice_EndSceneFunc_arrStolenBytes;

    static inline void* dxDevice_vTable[dxDevice_vTable_funcCount]{};

    static HRESULT __stdcall dxDevice_vTable_EndSceneFunc_hook(PDIRECT3DDEVICE9 pDxDevice)
    {
        pRenderFunc(pDxDevice);
        return pDxDevice_EndSceneFunc_org(pDxDevice);
    }

    static bool init_dxDevice_vTable() noexcept
    {
        // Create Direct3D9 object
        auto pDx = Direct3DCreate9(D3D_SDK_VERSION);
        if (!pDx)
        {
            return false;
        }

        // Define the presentation parameters
        D3DPRESENT_PARAMETERS dxParams{};
        dxParams.Windowed = true;
        dxParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
        dxParams.hDeviceWindow = hWnd;

        // Create a device
        PDIRECT3DDEVICE9 pDxDummyDevice = nullptr;
        auto result = pDx->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &dxParams, &pDxDummyDevice);
        if (FAILED(result))
        { // If failed, try changing the windowed mode
            dxParams.Windowed = !dxParams.Windowed;

            result = pDx->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &dxParams, &pDxDummyDevice);
            if (FAILED(result))
            { // If failed again, give up.
                pDx->Release();
                return false;
            }
        }

        // Copy the dummy device to the virtual table
        memcpy(dxDevice_vTable, *reinterpret_cast<void***>(pDxDummyDevice), sizeof(void*) * dxDevice_vTable_funcCount);

        // Release resources
        pDxDummyDevice->Release();
        pDx->Release();

        return true;
    }

public:
    static inline HWND hWnd;

    DirectX9Overlay(std::string strWindowName, std::function<void(PDIRECT3DDEVICE9)> _pRenderFunc, std::function<void()> _pCleanupFunc) noexcept
    {
        hWnd = FindWindowA(nullptr, strWindowName.c_str());
        pRenderFunc = _pRenderFunc;
        pCleanupFunc = _pCleanupFunc;

        // Create a dummy Direct3DDevice9 and copy it to the virtual table
        if (!init_dxDevice_vTable())
        {
            return;
        }

        // Hook!
        pDxDevice_EndSceneFunc_org = Detour::hook<dxDevice_EndSceneFunc_t, dxDevice_vTable_EndSceneFunc_nStolenBytes>
        (
            // pFunc_vTable
            dxDevice_vTable[dxDevice_vTable_EndSceneFunc_index],

            // pFunc_toHook
            dxDevice_vTable_EndSceneFunc_hook,

            // arrStolenBytes
            dxDevice_EndSceneFunc_arrStolenBytes
        );
    }
    
    ~DirectX9Overlay()
    {
        // Unhook!
        Detour::unhook<dxDevice_vTable_EndSceneFunc_nStolenBytes>
        (
            // pFunc_vTable
            dxDevice_vTable[dxDevice_vTable_EndSceneFunc_index],

            // arrStolenBytes
            dxDevice_EndSceneFunc_arrStolenBytes
        );

        // Cleanup!
        pCleanupFunc();
    }
};