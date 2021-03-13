#include "directx9_overlay.hpp"
#include "trampoline_hook.hpp"

#include <d3d9.h>
#include <d3dx9.h>

typedef HRESULT(__stdcall* dxDevice_EndSceneFunc_t)(PDIRECT3DDEVICE9);

static std::function<void(PDIRECT3DDEVICE9 pDxDevice)> pRenderFunc;
static std::function<void()> pCleanupFunc;

static dxDevice_EndSceneFunc_t pDxDevice_EndSceneFunc_org;
static std::array<uint8_t, DirectX9Overlay::dxDevice_vTable_EndSceneFunc_nStolenBytes> dxDevice_EndSceneFunc_arrStolenBytes;

static HRESULT __stdcall dxDevice_vTable_EndSceneFunc_hook(PDIRECT3DDEVICE9 pDxDevice)
{
    pRenderFunc(pDxDevice);
    return pDxDevice_EndSceneFunc_org(pDxDevice);
}

/////////////////////////////////////////////////////
 
HWND DirectX9Overlay::hWnd;

DirectX9Overlay::DirectX9Overlay(std::string strWindowName, std::function<void(PDIRECT3DDEVICE9)> _pRenderFunc, std::function<void()> _pCleanupFunc) noexcept
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
    pDxDevice_EndSceneFunc_org = TrampolineHook::hook<dxDevice_EndSceneFunc_t, dxDevice_vTable_EndSceneFunc_nStolenBytes>
    (
        // pFunc_vTable
        dxDevice_vTable[dxDevice_vTable_EndSceneFunc_index],

        // pFunc_toHook
        dxDevice_vTable_EndSceneFunc_hook,

        // arrStolenBytes
        dxDevice_EndSceneFunc_arrStolenBytes
    );
}

DirectX9Overlay::~DirectX9Overlay()
{
    // Unhook!
    TrampolineHook::unhook<dxDevice_vTable_EndSceneFunc_nStolenBytes>
    (
        // pFunc_vTable
        dxDevice_vTable[dxDevice_vTable_EndSceneFunc_index],

        // arrStolenBytes
        dxDevice_EndSceneFunc_arrStolenBytes
    );

    // Cleanup!
    pCleanupFunc();
}

/////////////////////////////////////////////////////

bool DirectX9Overlay::init_dxDevice_vTable() noexcept
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
    memcpy(dxDevice_vTable, *reinterpret_cast<void***>(pDxDummyDevice), sizeof(void*) *dxDevice_vTable_funcCount);

    // Release resources
    pDxDummyDevice->Release();
    pDx->Release();

    return true;
}
