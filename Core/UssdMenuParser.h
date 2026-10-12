#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace wan::core
{
    struct UssdOption
    {
        std::wstring key;   // what to send back, e.g. L"1"
        std::wstring label; // text shown on the button
    };

    // Best-effort extraction of numbered menu options from a USSD reply.
    // A segment is an option when it starts with 1–2 ASCII digits followed by '.', ')', '-', ':'
    // or whitespace, then a non-empty label. Segments are lines, and parts of a line separated by
    // two or more spaces. A '.' directly followed by a digit is a decimal ("1.5GB"), not an option.
    // Fewer than two options means "not a menu": returns empty.
    std::vector<UssdOption> ParseUssdMenu(std::wstring_view text);
}
