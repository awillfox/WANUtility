#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace wan::core
{
    // Process-wide UTF-8 log. When the file exceeds maxBytes it is renamed to "<file>.1"
    // (replacing any previous one) and a new file is started. Safe to call from any thread.
    class Log
    {
    public:
        static void Init(std::filesystem::path const& file, std::uintmax_t maxBytes = 1'000'000);
        static void Write(std::wstring_view line);
        static std::string ToUtf8(std::wstring_view text);
    };
}
