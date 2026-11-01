#pragma once
#include <windows.h>
#include <cstdint>
#include <string_view>

namespace sig {
inline uintptr_t Module(const wchar_t* name) {
    return (uintptr_t)GetModuleHandleW(name);
}
// pattern: "48 8B 05 ? ? ? ? 48 85 C0"
inline uintptr_t Scan(std::string_view pat, uintptr_t base, size_t len) {
    auto* p = (uint8_t*)base;
    size_t n = pat.size() / 3 + 1;
    for (size_t i = 0; i + n < len; ++i) {
        bool hit = true;
        for (size_t j = 0, k = 0; j < pat.size(); ++j, ++k) {
            if (pat[j] == ' ') { --k; continue; }
            if (pat[j] == '?') continue;
            auto hex = [](char c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; };
            if (p[i + k] != (uint8_t)(hex(pat[j]) << 4 | hex(pat[j + 1]))) { hit = false; break; }
            ++j;
        }
        if (hit) return base + i;
    }
    return 0;
}
} // namespace sig

// ---- UPDATE THESE EVERY ROBLOX PATCH ----------------------------------
namespace off {
constexpr uintptr_t TaskScheduler = 0x0;   // sig -> deref (TaskScheduler**)
constexpr uintptr_t Jobs         = 0x1A0;  // TaskScheduler::jobs (std::vector)
constexpr uintptr_t JobName      = 0x18;   // Job::name (std::string)
} // namespace off