#pragma once
#include <chrono>
#include <ctime>
#include <string>

namespace wan
{
    inline int64_t NowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    inline int64_t ToUnixMs(winrt::Windows::Foundation::DateTime t)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(winrt::clock::to_sys(t).time_since_epoch()).count();
    }

    inline winrt::Windows::Foundation::DateTime FromUnixMs(int64_t ms)
    {
        return winrt::clock::from_sys(std::chrono::system_clock::time_point(std::chrono::milliseconds(ms)));
    }

    // "13:54" for today, "12/10 13:54" otherwise (local time).
    inline std::wstring FormatTime(int64_t ms)
    {
        const time_t secs = static_cast<time_t>(ms / 1000);
        const time_t now = time(nullptr);
        tm t{};
        tm n{};
        localtime_s(&t, &secs);
        localtime_s(&n, &now);
        wchar_t buf[32];
        const bool today = t.tm_year == n.tm_year && t.tm_yday == n.tm_yday;
        wcsftime(buf, 32, today ? L"%H:%M" : L"%d/%m %H:%M", &t);
        return buf;
    }
}
