#include "Log.h"

#include <fstream>
#include <mutex>
#include <windows.h>

namespace wan::core
{
    namespace
    {
        std::mutex g_mutex;
        std::filesystem::path g_file;
        std::uintmax_t g_maxBytes = 1'000'000;
    }

    std::string Log::ToUtf8(std::wstring_view text)
    {
        if (text.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
        return out;
    }

    void Log::Init(std::filesystem::path const& file, std::uintmax_t maxBytes)
    {
        std::lock_guard lock(g_mutex);
        g_file = file;
        g_maxBytes = maxBytes;
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
    }

    void Log::Write(std::wstring_view line)
    {
        std::lock_guard lock(g_mutex);
        if (g_file.empty()) return;

        std::error_code ec;
        if (std::filesystem::exists(g_file, ec) && std::filesystem::file_size(g_file, ec) > g_maxBytes)
        {
            std::filesystem::rename(g_file, g_file.wstring() + L".1", ec); // replaces an existing .1
        }

        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t stamp[32];
        swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u.%03u ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

        std::ofstream f(g_file, std::ios::app | std::ios::binary);
        f << ToUtf8(stamp) << ToUtf8(line) << "\n";
    }
}
