#pragma once
#include <string>
#include <string_view>

namespace wan
{
    // "0x80070005 Access is denied." — the hex code is always included (the spike showed
    // some codes, e.g. 0xD0000022, have no system message text).
    inline std::wstring DescribeError(winrt::hresult_error const& e)
    {
        wchar_t code[16];
        swprintf_s(code, L"0x%08X", static_cast<uint32_t>(e.code().value));
        std::wstring message{ e.message() };
        return message.empty() ? std::wstring(code) : std::wstring(code) + L" " + message;
    }

    inline std::wstring Widen(std::string_view utf8)
    {
        if (utf8.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), size);
        return out;
    }
}
