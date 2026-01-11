#include "ViewAPI.hpp"

#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <AppCore/JSHelpers.h>
#include <FileWatcher/FileWatcher.h>
#include "FNV1A.hpp"

struct FileWatcherPrivate
{
    uint64_t hash;
    std::filesystem::path filepath;
    FW::WatchID watchID;
    FileWatcherPrivate(std::wstring path);
    ~FileWatcherPrivate();
    std::string ReadContent()
    {
        if (!std::filesystem::exists(filepath))
            return {"File not found"};
        std::ifstream inp(filepath);
        inp.seekg(0, std::ifstream::end);
        auto size = inp.tellg();
        inp.seekg(0, std::ifstream::beg);
        std::string content;
        content.resize(size);
        inp.read(content.data(), size);
        return content;
    }
};

extern RefPtr<View> view;

static std::unordered_set<std::filesystem::path> watchedFolders;
static std::unordered_map<uint64_t, FileWatcherPrivate*> watchers;

JSObjectRef FuncOnFileUpdated{};

class UpdateListenerJS : public FW::FileWatchListener
{
public:
    UpdateListenerJS() {}
    void handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename, FW::Action action)
    {
        FNV_1A hsh;
        hsh.AppendBytes((char*)dir.data(), dir.length() * sizeof(wchar_t));
        hsh.AppendBytes((char*)filename.data(), filename.length() * sizeof(wchar_t));
        auto hash = hsh.GetHash();

        auto found = watchers.find(hash);
        if (found != watchers.end())
        {
            auto scoped_context = view->LockJSContext();

            // Typecast to the underlying JSContextRef.
            JSContextRef ctx = (*scoped_context);

            constexpr size_t buf_size = 32;
            char buf[buf_size]{};
            std::to_chars_result result = std::to_chars(buf, buf + buf_size, found->second->hash, 10);

            const JSValueRef args[] = {
                JSValueMakeString(ctx, JSString(ultralight::String(buf, result.ptr - buf)))
            };

            JSValueRef exception;
            JSObjectCallAsFunction(ctx, FuncOnFileUpdated, 0, std::size(args), args, &exception);
        }

    }
};


static UpdateListenerJS listener;
static FW::FileWatcher fileWatcher;


FileWatcherPrivate::FileWatcherPrivate(std::wstring path): filepath(path)
{
    if (!std::filesystem::exists(path))
        return;

    auto parentFolder = filepath.parent_path().wstring();
    FNV_1A hsh;
    hsh.AppendBytes((char*)parentFolder.data(), parentFolder.length() * sizeof(wchar_t));
    auto fn = filepath.filename().wstring();
    hsh.AppendBytes((char*)fn.data(), fn.length() * sizeof(wchar_t));
    hash = hsh.GetHash();
    watchers[hash] = this;
    if (watchedFolders.contains(parentFolder))
        return;

    watchID = fileWatcher.addWatch(FW::String(parentFolder), &listener, false);
    watchedFolders.emplace(parentFolder);
}

FileWatcherPrivate::~FileWatcherPrivate()
{
    //fileWatcher.removeWatch(watchID);
    watchers.erase(hash);
}


std::optional<JSObjectRef> TryGetFunction(const char* name, JSContextRef& ctx)
{
    // Get the ShowMessage function by evaluating a script. We could have
    // also used JSContextGetGlobalObject() and JSObjectGetProperty() to 
    // retrieve this from the global window object as well.

    // Create our string of JavaScript, automatically managed by JSRetainPtr
    JSStringRef str = JSStringCreateWithUTF8CString(name);

    JSValueRef exception = 0;
    JSValueRef func = JSObjectGetProperty(ctx, JSContextGetGlobalObject(ctx), str, &exception);

    // Check if 'func' is actually an Object and not null
    if (JSValueIsObject(ctx, func)) {

        // Cast 'func' to an Object, will return null if typecast failed.
        JSObjectRef funcObj = JSValueToObject(ctx, func, 0);

        // Check if 'funcObj' is a Function and not null
        if (funcObj && JSObjectIsFunction(ctx, funcObj)) {

            return funcObj;
        }
    }
    return {};
}

void APIInjector::OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url)
{
    // Acquire the JS execution context for the current page.
    auto scoped_context = caller->LockJSContext();

    // Typecast to the underlying JSContextRef.
    JSContextRef ctx = (*scoped_context);


    if (auto fnc = TryGetFunction("OnFileUpdated", ctx))
    {
        FuncOnFileUpdated = std::move(*fnc);
        JSValueProtect(ctx, FuncOnFileUpdated); //#TODO we never free this, because we probably can't get context in destructor. Maybe some on closing event?
    }

    static JSStaticValue staticValues[] = {
    { "content",
        [](JSContextRef ctx, JSObjectRef object,JSStringRef propertyName, JSValueRef* exception)->JSValueRef {
            FileWatcherPrivate* fs = static_cast<FileWatcherPrivate*>(JSObjectGetPrivate(object));
            
            auto data = fs->ReadContent();
            return JSValueMakeString(ctx, JSString(data.c_str()));
        }, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "hash",
        [](JSContextRef ctx, JSObjectRef object,JSStringRef propertyName, JSValueRef* exception)->JSValueRef {
            FileWatcherPrivate* fs = static_cast<FileWatcherPrivate*>(JSObjectGetPrivate(object));

            constexpr size_t buf_size = 32;
            char buf[buf_size]{};
            std::to_chars_result result = std::to_chars(buf, buf + buf_size, fs->hash, 10);

            return JSValueMakeString(ctx, JSString(buf));
        }, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },

    { 0, 0, 0, 0 }
    };


    static JSClassDefinition fileWatcherDef{
            .version = 0,
            .attributes = kJSClassAttributeNone,
            .className = "FileWatcher",

            .staticValues = staticValues,
            .finalize = [](JSObjectRef object)
            {
                FileWatcherPrivate* fs = static_cast<FileWatcherPrivate*>(JSObjectGetPrivate(object));
                delete fs;
            },
            //.getProperty = [](JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) -> JSValueRef
            //{
            //    return JSValueMakeNull(ctx);
            //},
            .callAsConstructor = [](JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSObjectRef
            {
                JSObjectRef newObj = JSObjectMake(ctx, JSClassCreate(&fileWatcherDef), nullptr);
                JSStringRef pathString = JSValueToStringCopy(ctx, arguments[0], nullptr);
                std::wstring path = static_cast<String>(JSString(pathString)).utf16().data();
                FileWatcherPrivate* fs = new FileWatcherPrivate(path);
                JSObjectSetPrivate(newObj, static_cast<void*>(fs));

                return newObj;
            }
    };


    
    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);
    JSObjectRef filesystemObject = JSObjectMake(ctx, JSClassCreate(&fileWatcherDef), nullptr);
    JSObjectSetProperty(ctx, globalObject, JSStringCreateWithUTF8CString("FileWatcher"), filesystemObject, kJSPropertyAttributeNone, nullptr);


    if (auto fnc = TryGetFunction("OnInit", ctx))
    {
        JSValueRef exception;
        JSObjectCallAsFunction(ctx, *fnc, 0, 0, {}, &exception);
    }

}

void APIInjector::OnBeginLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url)
{
    FuncOnFileUpdated = {};
}
