#pragma once
// These FNV-1a utility functions are extremely performance sensitive,
// check examples like that in VSO-653642 before making changes.
#include <cstdint>
#include <type_traits>
#if defined(_WIN64)
constexpr size_t _FNV_offset_basis = 14695981039346656037ULL;
constexpr size_t _FNV_prime = 1099511628211ULL;
#else // ^^^ defined(_WIN64) / !defined(_WIN64) vvv
constexpr size_t _FNV_offset_basis = 2166136261U;
constexpr size_t _FNV_prime = 16777619U;
#endif // ^^^ !defined(_WIN64) ^^^

class FNV_1A {
    std::uint64_t value{ _FNV_offset_basis };
public:

    constexpr FNV_1A() {}

    constexpr void AppendBytes(const char* const _First, const size_t _Count) noexcept { // accumulate range [_First, _First + _Count) into partial FNV-1a hash _Val
        for (size_t _Idx = 0; _Idx < _Count; ++_Idx) {
            value ^= static_cast<size_t>(_First[_Idx]);
            value *= _FNV_prime;
        }
    }

    constexpr void AppendBytes(const char8_t* const _First, const size_t _Count) noexcept { // accumulate range [_First, _First + _Count) into partial FNV-1a hash _Val
        for (size_t _Idx = 0; _Idx < _Count; ++_Idx) {
            value ^= static_cast<size_t>(_First[_Idx]);
            value *= _FNV_prime;
        }
    }

    template <class _Ty>
    constexpr void AppendRange(const _Ty* const _First, const _Ty* const _Last) noexcept { // accumulate range [_First, _Last) into partial FNV-1a hash _Val
        static_assert(std::is_trivially_copyable_v<_Ty>, "Only trivially copyable types can be directly hashed.");
        const auto _Firstb = reinterpret_cast<const unsigned char*>(_First);
        const auto _Lastb = reinterpret_cast<const unsigned char*>(_Last);
        AppendBytes(_Firstb, static_cast<size_t>(_Lastb - _Firstb));
    }

    template <class _Kty>
    constexpr void AppendValue(const _Kty& _Keyval) noexcept { // accumulate _Keyval into partial FNV-1a hash _Val
        static_assert(std::is_trivially_copyable_v<_Kty>, "Only trivially copyable types can be directly hashed.");
        AppendBytes(&reinterpret_cast<const unsigned char&>(_Keyval), sizeof(_Kty));
    }

    constexpr uint64_t GetHash() const noexcept
    {
        return value;
    }
};
