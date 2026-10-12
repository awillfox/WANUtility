#pragma once
#include <string>
#include <string_view>

namespace wan::core
{
    // Thread key for an SMS address so that one contact maps to one thread.
    //  - Thai local numbers "0XXXXXXXX(X)" -> "+66XXXXXXXX(X)"
    //  - "66XXXXXXXXX" (11 digits)        -> "+66XXXXXXXXX"
    //  - "+..." numbers                    -> "+" + digits
    //  - spaces, dashes, dots, parentheses inside numbers are dropped
    //  - other numeric strings (short codes) -> digits as-is
    //  - alphanumeric senders ("dtac")     -> trimmed, verbatim
    std::wstring ThreadKeyFor(std::wstring_view address);

    // True when the key is a number we can send an SMS to ("+" optional, then digits only).
    bool IsPhoneNumberKey(std::wstring_view key);
}
