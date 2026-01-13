#include <Windows.h>
#include <Psapi.h>
#pragma comment (lib, "Psapi.lib")//GetModuleInformation
#pragma comment (lib, "version.lib")
#pragma comment (lib, "DXGI.lib")

#include <cstdint>
#include <string_view>

#pragma comment (lib, "AppCore.lib")
#pragma comment (lib, "Ultralight.lib")
#pragma comment (lib, "UltralightCore.lib")
#pragma comment (lib, "WebCore.lib")

#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>

#include <FileWatcher/FileWatcher.h>

#include "MemorySection.hpp"
#include "ViewAPI.hpp"


#include "HookManager.hpp"
using namespace std::string_view_literals;

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


typedef HRESULT(*FUN_Present)(IDXGISwapChain4* This, UINT SyncInterval, UINT PresentFlags);
static HRESULT FAKE_Present(IDXGISwapChain4* This, UINT SyncInterval, UINT PresentFlags);
static FUN_Present OLD_Present = nullptr;

#define USE_ULTRALIGHT

static HWND                     g_hWnd = nullptr;
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
    {
        // See if we should reload our main view
        fileWatcher.update();


        UltralightDraw();
    }

    return OLD_Present(pSwapChain, SyncInterval, PresentFlags);
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



HookManager::Pattern pat_DXGIPresent{
    "xxxxxxxx?xxxxxxxxxx????xxxxxx?xxxxxxx?xxx????xxxxxxxx?xxxxxx?xxx??xx????xx?????xx"sv,
    "\x55\x57\x41\x56\x48\x8D\x6C\x24\x00\x48\x81\xEC\x70\x01\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\x00\x45\x33\xF6\x44\x89\x44\x24\x00\x44\x39\x35\x00\x00\x00\x00\x41\x8B\xF0\x8B\xFA\x89\x54\x24\x00\x48\x8B\xD9\x48\x89\x4D\x00\xC6\x44\x24\x00\x00\x0F\x85\x00\x00\x00\x00\xF6\x05\x00\x00\x00\x00\x00\x0F\x85"sv,
    -0xA // Not going to start, because the start generally gets hooked with a jmp by other overlays
};

DWORD WINAPI hookThread(LPVOID lpParam) {

    uintptr_t D3DBase = 0;
    uintptr_t DXGIBase = 0;
    HookManager hookMan;
    MemorySections sections;

    while (!D3DBase || !DXGIBase) {        //Have to wait till d3d9.dll is loaded
        MODULEINFO modInfo = { 0 };
        HMODULE hModule = GetModuleHandle(L"d3d12.dll");
        GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
        D3DBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);

        hModule = GetModuleHandle(L"dxgi.dll");
        GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
        DXGIBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
        hookMan.engineBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
        hookMan.engineSize = static_cast<uintptr_t>(modInfo.SizeOfImage);
        sections = MemorySections(modInfo);
        Sleep(100);
    }
        


    // Find DXGI Present function

    auto dxPresent = hookMan.findPattern(pat_DXGIPresent);
    if (!dxPresent)
    {
        MessageBoxA(0, "ETSOverlay failed to find Present function", "ETSOverlay failed to find Present function", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND);
        return 0;
    }

    // There should be 3 references to it.
    // We want CMTUseCountedObject<class CDXGISwapChainApplicationClass>

    for (auto& sec : sections.sections)
    {
        // We only want .rdata, which is R
        if (sec.access & (MemorySection::W | MemorySection::X))
            continue;

        std::optional<uintptr_t> found;
        while (found = sec.findInMemory((char*)&dxPresent, sizeof(dxPresent), found ? *found - sec.start : 0))
        {
            // We found a reference to present, it is in 3 vtables. We want to only hook the first but whatever just do all for safety
            // Replacing multiple doesn't matter because OLD_Present is always the same
            OLD_Present = hack_vtable<FUN_Present>(reinterpret_cast<FUN_Present*>(*found), FAKE_Present);
        }

    }

    return 0;
}




// Be a plugin so the game loads us automatically

#include "lib/scs_sdk_1_14/include/scssdk_telemetry.h"

/**
 * @brief Telemetry API initialization function.
 *
 * See scssdk_telemetry.h
 */
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params)
{
    return SCS_RESULT_ok;
}

/**
 * @brief Telemetry API deinitialization function.
 *
 * See scssdk_telemetry.h
 */
SCSAPI_VOID scs_telemetry_shutdown(void)
{
    __nop();
    //#TODO cleanup/shutdown browser

    view.reset();
    renderer.reset();
}






BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    //WAIT_FOR_DEBUGGER_ATTACHED;
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            //MessageBoxA(0, "WAIT1", "WAIT1", 0);
            DWORD  dwThreadId;
            CreateThread(NULL, 0, hookThread, NULL, 0, &dwThreadId); //Have to wait till d3d9.dll is loaded
        }
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            __nop();
            break;
    }

    return TRUE;
}


//todo
//if we implement the plugin API exports for ETS telemetry, it will load it without needing an injector
//
//========== Plugin modules ==========
//
//The plugins are implemented as a dynamically loaded libraries(DLL on Windows, SO on Linux) which MUST export set of functions required by the APIs they want to use.The game loads all plugins found by combination of following methods :
//
//1) By gathering all libraries from "plugins" subdirectory of the directory containing the game binary(e.g. / bin / win_x86 / plugins).
//2) (Windows only) By processing all string values stored in registry key "HKEY_LOCAL_MACHINE\SOFTWARE\SCS Software\<GAME_NAME>\Plugins" (GAME_NAME is "Euro Truck Simulator 2" for ETS2 and "American Truck Simulator" for ATS).When targeting 32bit version of the game on 64bit OS, the registry key is stored in "HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\SCS Software\<GAME_NAME>\Plugins".The data of those values are interpreted as paths to the libraries, one path per value.The names of the values are not relevant however it is recommended to use something identifying your product to avoid possible conflicts.When installing your product you can create this value including all the keys in the path if they do not exist yet to allow the game to find the plugin even if the user installs the game latter.When uninstalling your product you should remove the value.The keys can be removed if and only if they are empty.
