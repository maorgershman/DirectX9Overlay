#pragma once

#include <cstdint>
#include <array>

#include <Windows.h>

struct TrampolineHook
{
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

        memcpy(static_cast<PBYTE>(pFunc_gateway) + nStolenBytes, &JMP, sizeof(JMP));
        memcpy(static_cast<PBYTE>(pFunc_gateway) + nStolenBytes + sizeof(JMP), &gatewayRelativeAddress, sizeof(gatewayRelativeAddress));

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
        memcpy(static_cast<PBYTE>(pFunc_vTable) + sizeof(JMP), &relativeAddress, sizeof(relativeAddress));

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
        memcpy(static_cast<PBYTE>(pFunc_vTable), arrStolenBytes.data(), nStolenBytes);

        DWORD dwTemp;
        VirtualProtect(pFunc_vTable, nStolenBytes, dwProtection, &dwTemp);
    }

private:
    static constexpr uint8_t JMP = 0xE9;
    static constexpr uint8_t NOP = 0x90;
};