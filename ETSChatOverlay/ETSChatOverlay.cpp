#include <cstdint>
#include <Windows.h>
#include <Psapi.h>
#pragma comment (lib, "Psapi.lib")//GetModuleInformation
#pragma comment (lib, "version.lib")
#include <d3d9.h>
#include <d3dx9.h>
#include <fstream>

#include <thread>

extern "C" {
    uintptr_t dxDevice;
    uintptr_t presentHookJmpBack;
    uintptr_t endSceneHookJmpBack;
    uintptr_t D3DendSceneHookJmpBack;
    void dxHookFunc();
    void presentHook();
    void endSceneHook();
    void D3DendSceneHook();
    void D3DendSceneHook7();
}


ID3DXFont* font = nullptr;

void dxHookFunc() {
    auto dev = reinterpret_cast<LPDIRECT3DDEVICE9>(dxDevice);
    if (!font) {
        auto res = D3DXCreateFontA(dev, 16, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, FF_DONTCARE, "Arial", &font);
        if (!SUCCEEDED(res)) __debugbreak();
    }

    //RECT drawRect{0,0,300,200};
    //RECT drawRect{ 2420, 30, 530, 400 }; // doesn't render at x=2420!? or anything > 500
    //SetRect(&drawRect, 0, 0, 300, 200);

    RECT topleft{ 28, 28, 530, 400 };
    RECT topright{ 32, 28, 530, 400 };
    RECT bottomleft{ 28, 32, 530, 400 };
    RECT bottomright{ 32, 32, 530, 400 };
    RECT drawRect{ 30, 30, 530, 400 };

    std::ifstream st("text.txt");
    std::string str((std::istreambuf_iterator<char>(st)),
        std::istreambuf_iterator<char>());

    // create black "outline"
    // couldn't figure out how to render to a texture once when the TXT changes,
    // and then just draw the texture *sadface* -jack
    font->DrawTextA(NULL, str.c_str(), -1, &topleft, DT_LEFT | DT_WORDBREAK, D3DCOLOR_ARGB(255, 0, 0, 0));
    font->DrawTextA(NULL, str.c_str(), -1, &topright, DT_LEFT | DT_WORDBREAK, D3DCOLOR_ARGB(255, 0, 0, 0));
    font->DrawTextA(NULL, str.c_str(), -1, &bottomleft, DT_LEFT | DT_WORDBREAK, D3DCOLOR_ARGB(255, 0, 0, 0));
    font->DrawTextA(NULL, str.c_str(), -1, &bottomright, DT_LEFT | DT_WORDBREAK, D3DCOLOR_ARGB(255, 0, 0, 0));

    font->DrawTextA(NULL, str.c_str(), -1, &drawRect, DT_LEFT | DT_WORDBREAK, D3DCOLOR_ARGB(255, 255, 255, 0));
}

uintptr_t placeHookTotalOffs(uintptr_t totalOffset, uintptr_t jmpTo) {
    DWORD dwVirtualProtectBackup;


    /*
    32bit
    jmp 0x123122
    0:  e9 1e 31 12 00          jmp    123123 <_main+0x123123>
    64bit
    FF 25 64bit relative
    */

    //auto distance = std::max(totalOffset, jmpTo) - std::min(totalOffset, jmpTo);
    // if distance < 2GB (2147483648) we could use the 32bit relative jmp
    VirtualProtect(reinterpret_cast<LPVOID>(totalOffset), 14u, 0x40u, &dwVirtualProtectBackup);
    auto jmpInstr = reinterpret_cast<unsigned char*>(totalOffset);
    auto addrOffs = reinterpret_cast<uint32_t*>(totalOffset + 1);
    *jmpInstr = 0x68; //push DWORD
    *addrOffs = static_cast<uint32_t>(jmpTo) /*- totalOffset - 6*/;//offset
    *reinterpret_cast<uint32_t*>(totalOffset + 5) = 0x042444C7; //MOV [RSP+4],
    *reinterpret_cast<uint32_t*>(totalOffset + 9) = static_cast<uint64_t>(jmpTo) >> 32;//DWORD
    *reinterpret_cast<unsigned char*>(totalOffset + 13) = 0xc3;//ret
    VirtualProtect(reinterpret_cast<LPVOID>(totalOffset), 14u, dwVirtualProtectBackup, &dwVirtualProtectBackup);
    return totalOffset + 14;


}

#ifdef _DEBUG
#define WAIT_FOR_DEBUGGER_ATTACHED while (!IsDebuggerPresent()) Sleep(1);
#else
#define WAIT_FOR_DEBUGGER_ATTACHED 
#endif

DWORD WINAPI hookThread(LPVOID lpParam) {

        uintptr_t D3DBase = 0;
        HMODULE hModule = 0;
        while (!D3DBase) {        //Have to wait till d3d9.dll is loaded
            MODULEINFO modInfo = { 0 };
            hModule = GetModuleHandle(L"d3d9.dll");
            GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
            D3DBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
            Sleep(100);
        }


        WCHAR fileName[_MAX_PATH];
        DWORD size = GetModuleFileName(hModule, fileName, _MAX_PATH);
        fileName[size] = NULL;
        DWORD handle = 0;
        size = GetFileVersionInfoSize(fileName, &handle);
        BYTE* versionInfo = new BYTE[size];
        if (!GetFileVersionInfo(fileName, handle, size, versionInfo)) {
            return 0;
        }
        UINT    			len = 0;
        VS_FIXEDFILEINFO*   vsfi = NULL;
        VerQueryValue(versionInfo, L"\\", (void**) &vsfi, &len);
        short version = HIWORD(vsfi->dwFileVersionLS);
        short minor = LOWORD(vsfi->dwFileVersionMS);
        delete[] versionInfo;
        if (minor == 3) {//win 8
            placeHookTotalOffs(D3DBase + 0xBA90, reinterpret_cast<uintptr_t>(D3DendSceneHook));
            D3DendSceneHookJmpBack = D3DBase + 0xBAA9;
        } else {//win 7
            placeHookTotalOffs(D3DBase + 0x2279F, reinterpret_cast<uintptr_t>(D3DendSceneHook7));
            D3DendSceneHookJmpBack = D3DBase + 0x227A6;
        }


        return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    //WAIT_FOR_DEBUGGER_ATTACHED;
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {

            MODULEINFO modInfo = { 0 };
            HMODULE hModule = GetModuleHandle(NULL);
            GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
            auto engineBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);


            DWORD  dwThreadId;
            CreateThread(NULL, 0, hookThread, NULL, 0, &dwThreadId);     //Have to wait till d3d9.dll is loaded





            //WCHAR fileName[_MAX_PATH];
            //DWORD size = GetModuleFileName(nullptr, fileName, _MAX_PATH);
            //fileName[size] = NULL;
            //auto fname = std::wstring(fileName).substr(size - 15);
            //if (fname.compare(L"eurotrucks2.exe") == 0) {
            //    // ETS2 1.28.1.3
            //    placeHookTotalOffs(engineBase + 0x74360, reinterpret_cast<uintptr_t>(endSceneHook));
            //    endSceneHookJmpBack = engineBase + 0x74374;
            //} else {
            //    // ATS 
            //    placeHookTotalOffs(engineBase + 0x67CE0, reinterpret_cast<uintptr_t>(endSceneHook));
            //    endSceneHookJmpBack = engineBase + 0x67CF4;
            //}




        }
        case DLL_THREAD_ATTACH: {

        }
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

// int main() {
// 
// 
//     return 0;
// }
