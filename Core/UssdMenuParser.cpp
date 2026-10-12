#include "UssdMenuParser.h"
#include "TextUtil.h"

#include <optional>

namespace wan::core
{
    namespace
    {
        bool IsDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }

        std::optional<UssdOption> ParseSegment(std::wstring_view s)
        {
            s = TrimView(s);
            size_t n = 0;
            while (n < s.size() && IsDigit(s[n])) ++n;
            if (n == 0 || n > 2 || n >= s.size())
            {
                return std::nullopt;
            }
            size_t i = n;
            const wchar_t sep = s[i];
            if (sep == L'.' || sep == L')' || sep == L'-' || sep == L':')
            {
                ++i;
                if (i < s.size() && IsDigit(s[i]))
                {
                    return std::nullopt; // "1.5GB" is a decimal, not option 1
                }
            }
            else if (sep != L' ' && sep != L'\t')
            {
                return std::nullopt;
            }
            const auto label = TrimView(s.substr(i));
            if (label.empty())
            {
                return std::nullopt;
            }
            return UssdOption{ std::wstring(s.substr(0, n)), std::wstring(label) };
        }

        // Splits a line on runs of two or more spaces ("1. ยืนยัน  2. ยกเลิก").
        std::vector<std::wstring_view> SplitSegments(std::wstring_view line)
        {
            std::vector<std::wstring_view> out;
            size_t start = 0;
            size_t i = 0;
            while (i < line.size())
            {
                if (line[i] == L' ' && i + 1 < line.size() && line[i + 1] == L' ')
                {
                    out.push_back(line.substr(start, i - start));
                    while (i < line.size() && line[i] == L' ') ++i;
                    start = i;
                }
                else
                {
                    ++i;
                }
            }
            out.push_back(line.substr(start));
            return out;
        }
    }

    std::vector<UssdOption> ParseUssdMenu(std::wstring_view text)
    {
        std::vector<UssdOption> options;
        size_t pos = 0;
        while (pos <= text.size())
        {
            const size_t end = text.find_first_of(L"\r\n", pos);
            const auto line = text.substr(pos, end == std::wstring_view::npos ? std::wstring_view::npos : end - pos);
            for (auto segment : SplitSegments(line))
            {
                if (auto option = ParseSegment(segment))
                {
                    options.push_back(*option);
                }
            }
            if (end == std::wstring_view::npos)
            {
                break;
            }
            pos = end + 1;
        }
        if (options.size() < 2)
        {
            options.clear();
        }
        return options;
    }
}
