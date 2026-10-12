#pragma once
#include <string>
#include <string_view>

namespace wan::core
{
    inline bool IsSpace(wchar_t c)
    {
        return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0x00A0;
    }

    inline std::wstring_view TrimView(std::wstring_view s)
    {
        while (!s.empty() && IsSpace(s.front())) s.remove_prefix(1);
        while (!s.empty() && IsSpace(s.back())) s.remove_suffix(1);
        return s;
    }

    inline std::wstring Trim(std::wstring_view s)
    {
        return std::wstring(TrimView(s));
    }

    // First line of s (up to CR or LF), trimmed.
    inline std::wstring FirstLine(std::wstring_view s)
    {
        return std::wstring(TrimView(s.substr(0, s.find_first_of(L"\r\n"))));
    }
}
