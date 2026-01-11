#include <cstdint>
#include <Windows.h>
#include <Psapi.h>
#pragma comment (lib, "Psapi.lib")//GetModuleInformation
#pragma comment (lib, "version.lib")
#pragma comment (lib, "DXGI.lib")



#pragma comment (lib, "AppCore.lib")
#pragma comment (lib, "Ultralight.lib")
#pragma comment (lib, "UltralightCore.lib")
#pragma comment (lib, "WebCore.lib")

#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>




#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN

#define NOGDICAPMASKS         //CC_ * , LC_*, PC_*, CP_*, TC_*, RC_
//#define NOVIRTUALKEYCODES     //VK_ *
//#define NOWINMESSAGES         //WM_ * , EM_*, LB_*, CB_*
//#define NOWINSTYLES           //WS_ * , CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS          //SM_ *
#define NOMENUS               //MF_ *
#define NOICONS               //IDI_ *
#define NOKEYSTATES           //MK_ *
#define NOSYSCOMMANDS         //SC_ *
#define NORASTEROPS           //Binary and Tertiary raster ops
#define NOSHOWWINDOW          //SW_ *
#define NOATOM                //Atom Manager routines
#define NOCLIPBOARD           //Clipboard routines
#define NOCOLOR               //Screen colors
#define NOCTLMGR              //Control and Dialog routines
#define NODRAWTEXT            //DrawText() and DT_*
#define NOGDI                 //All GDI defines and routines
#define NOKERNEL              //All KERNEL defines and routines
#define NONLS                 //All NLS defines and routines
//#define NOUSER                //All USER defines and routines
//#define NOMB                  //MB_ * and MessageBox()
#define NOMEMMGR              //GMEM_ * , LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE            //typedef METAFILEPICT
#define NOMINMAX              //Macros min(a, b) and max(a, b)
//#define NOMSG                 //typedef MSG and associated routines
#define NOOPENFILE            //OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL              //SB_ * and scrolling routines
#define NOSERVICE             //All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND               //Sound driver routines
#define NOTEXTMETRIC          //typedef TEXTMETRIC and associated routines
#define NOWH                  //SetWindowsHook and WH_*
//#define NOWINOFFSETS          //GWL_ * , GCL_*, associated routines
#define NOCOMM                //COMM driver routines
#define NOKANJI               //Kanji support stuff.
#define NOHELP                //Help engine interface.
#define NOPROFILER            //Profiler interface.
#define NODEFERWINDOWPOS      //DeferWindowPos routines
#define NOMCX                 //Modem Configuration Extensions

#include <cassert>
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>

#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include <FileWatcher/FileWatcher.h>

#include "ViewAPI.hpp"


#define EXPORT extern "C"

//extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT(*OLD_Wndproc)(HWND, UINT, WPARAM, LPARAM) = nullptr;

[[noreturn]] static void fatal_error(const char* const msg) {
    MessageBoxA(NULL, msg, "Fatal Error", 0);
    abort();
}

template<typename T>
static inline T hack_vtable(T* old_fun, T new_fun) noexcept {
    T ret = *old_fun;
    DWORD perm;
    uintptr_t addr2 = (uintptr_t)old_fun;
    addr2 &= ~4095;
    VirtualProtect((void*)addr2, 4096, PAGE_EXECUTE_READWRITE, &perm);
    *old_fun = new_fun;
    VirtualProtect((void*)addr2, 4096, perm, nullptr);
    return ret;
}

typedef void(*dummy_function)(void);


typedef HRESULT(*FUN_Present)(IDXGISwapChain4* This, UINT SyncInterval, UINT PresentFlags);
static HRESULT FAKE_Present(IDXGISwapChain4* This, UINT SyncInterval, UINT PresentFlags);
static FUN_Present OLD_Present = nullptr;

static bool should_show_ui = true;

//#define USE_IMGUI
#define USE_ULTRALIGHT

static HWND                     g_hWnd = nullptr;
#ifdef USE_IMGUI
static HMODULE					g_hModule = nullptr;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
#endif
static std::once_flag           g_isInitialized;





#include <Ultralight/Ultralight.h>
#include <AppCore/Platform.h>
//#include "lib/Ultralight/AppCore/src/common/OverlayManager.h"
#include "GPUDriverD3D11Overlay.hpp"
#include "GPUContextD3D11.hpp"
#include "SwapChainD3D11.hpp"
using namespace ultralight;
using namespace ultralightOverlay;

RefPtr<Renderer> renderer;
RefPtr<View> view;
GPUContextD3D11* gpu_context;
GPUDriverD3D11Overlay* gpu_driver;
SwapChainD3D11* gpu_SwapChain;

//OverlayManager overlayManager;

std::vector<ultralight::Vertex_2f_4ub_2f_2f_28f> vertices_;
std::vector<ultralight::IndexType> indices_;
bool needs_update_ = true;
uint32_t geometry_id_;
ultralight::GPUState gpu_state_;
static IndexType patternCW[] = { 0, 1, 3, 1, 2, 3 };
static IndexType patternCCW[] = { 0, 3, 1, 1, 3, 2 };

bool use_gpu_ = true;

void UpdateGeometry() {
    if (!use_gpu_)
        return;

    bool initial_creation = false;
    RenderTarget target = view->render_target();

    if (vertices_.empty()) {
        vertices_.resize(4);
        indices_.resize(6);

        auto& config = Platform::instance().config();
        if (config.face_winding == FaceWinding::Clockwise)
            memcpy(indices_.data(), patternCW, sizeof(IndexType) * indices_.size());
        else
            memcpy(indices_.data(), patternCCW, sizeof(IndexType) * indices_.size());

        memset(&gpu_state_, 0, sizeof(gpu_state_));
        Matrix identity;
        identity.SetIdentity();

        gpu_state_.viewport_width = gpu_SwapChain->back_buffer_width();
        gpu_state_.viewport_height = gpu_SwapChain->back_buffer_height();
        gpu_state_.transform = identity.GetMatrix4x4();
        gpu_state_.enable_blend = true;
        gpu_state_.enable_texturing = true;
        gpu_state_.shader_type = ShaderType::Fill;
        gpu_state_.render_buffer_id = 0; // Into swapchain
        gpu_state_.texture_1_id = target.texture_id;

        initial_creation = true;
    }

    if (!needs_update_)
        return;

    Vertex_2f_4ub_2f_2f_28f v;
    memset(&v, 0, sizeof(v));

    v.data0[0] = 1; // Fill Type: Image

    v.color[0] = 255;
    v.color[1] = 255;
    v.color[2] = 255;
    v.color[3] = 255;

    float left = static_cast<float>(0);
    float top = static_cast<float>(0);
    float right = static_cast<float>(0 + gpu_SwapChain->back_buffer_width());
    float bottom = static_cast<float>(0 + gpu_SwapChain->back_buffer_height());

    // TOP LEFT
    v.pos[0] = v.obj[0] = left;
    v.pos[1] = v.obj[1] = top;
    v.tex[0] = target.uv_coords.left;
    v.tex[1] = target.uv_coords.top;

    vertices_[0] = v;

    // TOP RIGHT
    v.pos[0] = v.obj[0] = right;
    v.pos[1] = v.obj[1] = top;
    v.tex[0] = target.uv_coords.right;
    v.tex[1] = target.uv_coords.top;

    vertices_[1] = v;

    // BOTTOM RIGHT
    v.pos[0] = v.obj[0] = right;
    v.pos[1] = v.obj[1] = bottom;
    v.tex[0] = target.uv_coords.right;
    v.tex[1] = target.uv_coords.bottom;

    vertices_[2] = v;

    // BOTTOM LEFT
    v.pos[0] = v.obj[0] = left;
    v.pos[1] = v.obj[1] = bottom;
    v.tex[0] = target.uv_coords.left;
    v.tex[1] = target.uv_coords.bottom;

    vertices_[3] = v;

    ultralight::VertexBuffer vbuffer;
    vbuffer.format = ultralight::VertexBufferFormat::_2f_4ub_2f_2f_28f;
    vbuffer.size = static_cast<uint32_t>(sizeof(ultralight::Vertex_2f_4ub_2f_2f_28f) * vertices_.size());
    vbuffer.data = (uint8_t*)vertices_.data();

    ultralight::IndexBuffer ibuffer;
    ibuffer.size = static_cast<uint32_t>(sizeof(ultralight::IndexType) * indices_.size());
    ibuffer.data = (uint8_t*)indices_.data();

    if (initial_creation) {
        geometry_id_ = gpu_driver->NextGeometryId();
        gpu_driver->CreateGeometry(geometry_id_, vbuffer, ibuffer);
    }
    else {
        gpu_driver->UpdateGeometry(geometry_id_, vbuffer, ibuffer);
    }

    needs_update_ = false;
}


class MyViewListener : public ViewListener
{
public:
    void OnChangeTitle(ultralight::View* caller, const String& title) override
    {
        __nop();
    }
    void OnAddConsoleMessage(ultralight::View* caller, const ultralight::ConsoleMessage& message) override
    {
        auto str = message.message();
        __nop();
        Platform::instance().logger()->LogMessage(LogLevel::Info, str);
    }
};

MyViewListener* viewListener = new MyViewListener();

void UltralightDraw()
{
    gpu_driver->BeginSynchronize();

    renderer->Update();
    renderer->RefreshDisplay(0);
    //overlayManager.Render();
    {
        View* toRender = view.get();
        renderer->RenderOnly(&toRender, 1);
    }
    gpu_driver->EndSynchronize();
    //view->set_needs_paint(true);
    //renderer->Render();

    if (gpu_driver->HasCommandsPending() || view->needs_paint()
        //|| (window_needs_repaint_ && !is_first_paint_)
        ) {
        gpu_context->BeginDrawing();
        gpu_driver->DrawCommandList();

        ///overlayManager.Paint();
        {
            UpdateGeometry();
            gpu_driver->DrawGeometry(geometry_id_, 6, 0, gpu_state_);
        }

        //gpu_SwapChain->PresentFrame();
        gpu_context->EndDrawing();
        //if (is_first_paint_)
        //    is_first_paint_ = false;
    }
    else
    {
        // We hit the Ultralight's 60fps limit, just re-render the last frame
        gpu_driver->DrawGeometry(geometry_id_, 6, 0, gpu_state_);
    }

    //window_needs_repaint_ = false;


    //
    //gpu_driver->DrawCommandList();


}


class UpdateListener : public FW::FileWatchListener
{
public:
    UpdateListener() {}
    void handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename,
        FW::Action action)
    {
        if (filename == L"overlay.html" && action == FW::Action::Modified)
        {
            // Reload overlay file

            std::ifstream inp("overlay.html");
            inp.seekg(0, std::ifstream::end);
            auto size = inp.tellg();
            inp.seekg(0, std::ifstream::beg);
            std::string content;
            content.resize(size);
            inp.read(content.data(), size);
            view->LoadHTML(String(content.data(), content.length()));

            //std::cout << "DIR (" << dir + ") FILE (" + filename + ") has event " << action << std::endl;
        }
    }
};

// Monitor for our root html file
static UpdateListener listener;
static FW::FileWatcher fileWatcher;



static HRESULT FAKE_Present(IDXGISwapChain4* pSwapChain, UINT SyncInterval, UINT PresentFlags) {


    std::call_once(g_isInitialized, [&]() {
        pSwapChain->GetHwnd(&g_hWnd);
#ifdef USE_IMGUI

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = true;
        ImGui::StyleColorsDark();

        pSwapChain->GetDevice(__uuidof(g_pd3dDevice), reinterpret_cast<void**>(&g_pd3dDevice));
        g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
#endif

#ifdef USE_ULTRALIGHT

        Config config;

        ///
        /// Let's set some custom global CSS to make our background
        /// purple by default.
        ///
        config.user_stylesheet = "body { background: transparent; }";
        //config.force_repaint = true;




        ///
        /// Pass our configuration to the Platform singleton so that
        /// the library can use it.
        ///
        Platform::instance().set_config(config);

        gpu_context = new GPUContextD3D11(pSwapChain);
        gpu_driver = new GPUDriverD3D11Overlay(gpu_context);

        gpu_SwapChain = new SwapChainD3D11(pSwapChain, gpu_context, gpu_driver, g_hWnd, 0,0, 1.0, false, false, false, 0);
        gpu_context->AddSwapChain(gpu_SwapChain);

        Platform::instance().set_gpu_driver(gpu_driver);

        Platform::instance().set_font_loader(GetPlatformFontLoader());

        ///
        /// Use the OS's native file loader, with a base directory of "."
        /// All file:/// URLs will load relative to this base directory.
        ///
        Platform::instance().set_file_system(GetPlatformFileSystem("."));

        ///
        /// Use the default logger (writes to a log file)
        ///
        Platform::instance().set_logger(GetDefaultLogger("ultralight.log"));

        renderer = Renderer::Create();

        ///
        /// Configure our View, make sure it uses the CPU renderer by
        /// disabling acceleration.
        ///
        ViewConfig view_config;
        view_config.is_accelerated = true;
        view_config.display_id = 0;
        view_config.enable_compositor = true;
        view_config.is_transparent = true;
        //view_config.enable_compositor_debug_info = true;

        ///
        /// Create an offscreen web view, 500 by 500 pixels large.
        ///
        view = renderer->CreateView(gpu_SwapChain->back_buffer_width(), gpu_SwapChain->back_buffer_height(), view_config, nullptr);

        ///
        /// Load raw HTML asynchronously into the view.
        ///

        view->set_view_listener(viewListener);
        view->set_load_listener(new APIInjector());

        fileWatcher.addWatch(L".", &listener, false);

        std::ifstream inp("overlay.html");
        inp.seekg(0, std::ifstream::end);
        auto size = inp.tellg();
        inp.seekg(0, std::ifstream::beg);
        std::string content;
        content.resize(size);
        inp.read(content.data(), size);
        view->LoadHTML(String(content.data(), content.length()));

#endif

        });


    // Render!
    if (should_show_ui) {
#ifdef USE_IMGUI
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();


        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif

        // See if we should reload our main view
        fileWatcher.update();


        UltralightDraw();
    }

    return OLD_Present(pSwapChain, SyncInterval, PresentFlags);
}

static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    //if ((uMsg == WM_KEYUP) && (wParam == VK_ESCAPE)) {
    //    should_show_ui = !should_show_ui;
    //    return true;
    //}

    //if (should_show_ui && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
    //    return true;
    //}

    return CallWindowProc((WNDPROC)OLD_Wndproc, hWnd, uMsg, wParam, lParam);
}















/*

extern "C" {
    uintptr_t dxDevice;
    uintptr_t EndSceneFunc;
    uintptr_t d3d9Base;
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
        //std::stringstream ss;
        //ss << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << EndSceneFunc -d3d9Base;
        //MessageBoxA(0, ss.str().c_str(), ss.str().c_str(), 0);
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
    RECT drawRect2{ 0, 0, 530, 400 };

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
    //font->DrawTextA(NULL, "loooooooool", -1, &drawRect2, DT_LEFT | DT_WORDBREAK, D3DCOLOR_ARGB(255, 255, 255, 0));
}
*/
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
    uintptr_t DXGIBase = 0;
        HMODULE hModule = 0;
        while (!D3DBase || !DXGIBase) {        //Have to wait till d3d9.dll is loaded
            MODULEINFO modInfo = { 0 };
            hModule = GetModuleHandle(L"d3d12.dll");
            GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
            D3DBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);

            hModule = GetModuleHandle(L"dxgi.dll");
            GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
            DXGIBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
            Sleep(100);
        }
        
        //MessageBoxA(0, "WAIT", "WAIT", 0);


        // Find the vtable for DXGI Swapchain, and patch the Present call

        // CMTUseCountedObject<class CDXGISwapChainApplicationClass>
        auto vtableAddr = DXGIBase + 0x000000018009A000 - 0x180000000;

        struct VTabl {
            dummy_function QueryInterface;
            dummy_function AddRef;
            dummy_function Release;
            dummy_function SetPrivateData;
            dummy_function SetPrivateDataInterface;
            dummy_function GetPrivateData;
            dummy_function GetParent;
            dummy_function GetDevice;
            FUN_Present Present;
            dummy_function GetBuffer;
            dummy_function SetFullscreenState;
            dummy_function GetFullscreenState;
            dummy_function GetDesc;
            dummy_function ResizeBuffers;
            dummy_function ResizeTarget;
            dummy_function GetContainingOutput;
            dummy_function GetFrameStatistics;
            dummy_function GetLastPresentCount;
            dummy_function GetDesc1;
            dummy_function GetFullscreenDesc;
            dummy_function GetHwnd;
            dummy_function Present1;
            dummy_function IsTemporaryMonoSupported;
            dummy_function GetRestrictToOutput;
            dummy_function SetBackgroundColor;
            dummy_function GetBackgroundColor;
            dummy_function SetRotation;
            dummy_function GetRotation;
            dummy_function SetSourceSize;
            dummy_function GetSourceSize;
            dummy_function SetMaximumFrameLatency;
            dummy_function GetMaximumFrameLatency;
            dummy_function GetFrameLatencyWaitableObject;
            dummy_function SetMatrixTransform;
            dummy_function GetMatrixTransform;
            dummy_function GetCurrentBackBufferIndex;
            dummy_function CheckColorSpaceSupport;
            dummy_function SetColorSpace1;
            dummy_function ResizeBuffers1;
        };
        auto* vTable = (VTabl*)vtableAddr;

        if (!OLD_Present) {
            OLD_Present = hack_vtable(&vTable->Present, FAKE_Present);
            hack_vtable(&vTable->Present1, (dummy_function)[](void)->void{fatal_error("Present1"); });;
        }


        IDXGIFactory4* dxgiFactory = nullptr;
        //if (FAKE_CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
        //    return false;
        //
        //dxgiFactory->Release();
        
        //WCHAR fileName[_MAX_PATH];
        //DWORD size = GetModuleFileName(hModule, fileName, _MAX_PATH);
        //fileName[size] = NULL;
        //DWORD handle = 0;
        //size = GetFileVersionInfoSize(fileName, &handle);
        //BYTE* versionInfo = new BYTE[size];
        //if (!GetFileVersionInfo(fileName, handle, size, versionInfo)) {
        //    return 0;
        //}
        //UINT    			len = 0;
        //VS_FIXEDFILEINFO*   vsfi = NULL;
        //VerQueryValue(versionInfo, L"\\", (void**) &vsfi, &len);
        //short version = HIWORD(vsfi->dwFileVersionLS);
        //short minor = LOWORD(vsfi->dwFileVersionMS);
        //delete[] versionInfo;
        //if (minor == 3) {//win 8
        //    placeHookTotalOffs(D3DBase + 0xBA90, reinterpret_cast<uintptr_t>(D3DendSceneHook));
        //    D3DendSceneHookJmpBack = D3DBase + 0xBAA9;
        //} else {//win 7
        //    placeHookTotalOffs(D3DBase + 0x10A28, reinterpret_cast<uintptr_t>(D3DendSceneHook7));
        //    D3DendSceneHookJmpBack = D3DBase + 0x10A3C;
        //}



        //MODULEINFO modInfo = { 0 };
        //hModule = GetModuleHandle(NULL);
        //GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
        //auto engineBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
        //
        //
        //d3d9Base = D3DBase;
        //
        //size = GetModuleFileName(nullptr, fileName, _MAX_PATH);
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









        return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    //WAIT_FOR_DEBUGGER_ATTACHED;
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {

            //MODULEINFO modInfo = { 0 };
            //HMODULE hModule = GetModuleHandle(NULL);
            //GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
            //auto engineBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
            
            //MessageBoxA(0, "WAIT1", "WAIT1", 0);
            DWORD  dwThreadId;
            CreateThread(NULL, 0, hookThread, NULL, 0, &dwThreadId); //Have to wait till d3d9.dll is loaded
        }
        case DLL_THREAD_ATTACH: {

        }
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}
