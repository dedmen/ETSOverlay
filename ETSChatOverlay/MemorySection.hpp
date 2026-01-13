#pragma once
#include <cstdint>
#include <optional>
#include <Psapi.h>
#include <vector>
#include <string_view>


class MemorySection {
public:
#if _WIN32 || _WIN64
    explicit MemorySection(const MODULEINFO& modInfo) noexcept : start(reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll)),
                                                                 end(static_cast<uintptr_t>(modInfo.SizeOfImage) + reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll)) {}
#endif  // _WIN32 || _WIN64
    MemorySection(uintptr_t _start, uintptr_t _end) noexcept : start(_start), end(_end) {}
    uintptr_t start;
    uintptr_t end;

    enum SectionAccess
    {
        R = 1,
        W = 2,
        X = 4
    };

    SectionAccess access {SectionAccess(0)};

    inline size_t size() const {
        return end - start;
    }
    std::optional<uintptr_t> findInMemory(const char* pattern, size_t patternLength, uintptr_t startOffset = 0) const {
        const uintptr_t base = start + startOffset;
        if (startOffset >= size() - patternLength) return std::nullopt;
        const auto sz = size() - startOffset;
        for (uintptr_t i = 0; i < sz - patternLength; i++) {
            bool found = true;
            for (uintptr_t j = 0; j < patternLength; j++) {
                found &= pattern[j] == *reinterpret_cast<char*>(base + i + j);
                if (!found) {
                    break;
                }
            }
            if (found) {
                return base + i;
            }
        }
        return std::nullopt;
    }
    std::optional<uintptr_t> findInMemoryPattern(const char* pattern, const char* mask, uintptr_t offset = 0) const {
        const uintptr_t base = start;
        const auto sz = size();
        const auto patternLength = strlen(mask);
        for (uintptr_t i = 0; i < sz - patternLength; i++) {
            bool found = true;
            for (uintptr_t j = 0; j < patternLength; j++) {
                found &= mask[j] == '?' || pattern[j] == *reinterpret_cast<char*>(base + i + j);
                if (!found) {
                    break;
                }
            }
            if (found) {
                return base + i + offset;
            }
        }
        return std::nullopt;
    }
};

class MemorySections {
public:
    std::vector<MemorySection> sections;

public:
    MemorySections() = default;
    MemorySections(const MODULEINFO& modInfo);

    explicit MemorySections(uintptr_t _start, uintptr_t _size) : sections({MemorySection(_start, _start + _size)}) {}
    uintptr_t findInMemory(const char* pattern, size_t patternLength) const {
        for (const auto& section : sections) {
            auto found = section.findInMemory(pattern, patternLength);
            if (found.has_value()) {
                return found.value();
            }
        }
        return 0;
    }
    uintptr_t findInMemory(const std::string_view& pattern) const {
        return findInMemory(pattern.data(), pattern.length());
    }
    uintptr_t findInMemoryPattern(const char* pattern, const char* mask, uintptr_t offset = 0) const {
        for (const auto& section : sections) {
            auto found = section.findInMemoryPattern(pattern, mask, offset);
            if (found.has_value()) {
                return found.value();
            }
        }
        return 0;
    }

    bool IsInAnySection(uintptr_t address) const {
        return GetSectionByAddress(address).has_value();
    }

    std::optional<MemorySection> GetSectionByAddress(uintptr_t address) const {
        for (const auto& section : sections) {
            if (section.start < address && address < section.end)
                return section;
        }
        return std::nullopt;
    }
};
