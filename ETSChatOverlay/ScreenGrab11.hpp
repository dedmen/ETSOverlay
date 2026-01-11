#pragma once

#include <d3d11_1.h>

#if defined(NTDDI_WIN10_FE) || defined(__MINGW32__)
#include <ocidl.h>
#else
#include <OCIdl.h>
#endif

#include <functional>


namespace DirectX
{
    HRESULT __cdecl SaveDDSTextureToFile(
        _In_ ID3D11DeviceContext* pContext,
        _In_ ID3D11Resource* pSource,
        _In_z_ const wchar_t* fileName) noexcept;

    HRESULT __cdecl SaveWICTextureToFile(
        _In_ ID3D11DeviceContext* pContext,
        _In_ ID3D11Resource* pSource,
        _In_ REFGUID guidContainerFormat,
        _In_z_ const wchar_t* fileName,
        _In_opt_ const GUID* targetFormat = nullptr,
        _In_ std::function<void __cdecl(IPropertyBag2*)> setCustomProps = nullptr,
        _In_ bool forceSRGB = false);
}
