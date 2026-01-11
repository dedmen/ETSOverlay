#pragma once
#include <Ultralight/Ultralight.h>

using namespace ultralight;


class APIInjector : public LoadListener
{
public:
    void OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;
    void OnBeginLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const String& url) override;
};
