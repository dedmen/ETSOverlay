#include <cstdint>
#include <Windows.h>
#include <Psapi.h>
#pragma comment (lib, "Psapi.lib")//GetModuleInformation

#include <d3d9.h>
#include <d3dx9.h>
#include <fstream>

extern "C" {
    uintptr_t dxDevice;
    uintptr_t presentHookJmpBack;
    uintptr_t endSceneHookJmpBack;
    void dxHookFunc();
    void presentHook();
    void endSceneHook();
}


ID3DXFont* font = nullptr;

void dxHookFunc() {
    if (!font) {
        auto dev = reinterpret_cast<LPDIRECT3DDEVICE9>(dxDevice);
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
            auto engineSize = static_cast<uintptr_t>(modInfo.SizeOfImage);

            //placeHookTotalOffs(engineBase + 0x73EF3, reinterpret_cast<uintptr_t>(presentHook));
            //presentHookJmpBack = engineBase + 0x73F09;
            placeHookTotalOffs(engineBase + 0x74360, reinterpret_cast<uintptr_t>(endSceneHook));
            endSceneHookJmpBack = engineBase + 0x74374;
            
                                                                     


        }
        case DLL_THREAD_ATTACH:
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
