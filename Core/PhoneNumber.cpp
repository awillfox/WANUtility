#include "PhoneNumber.h"
#include "TextUtil.h"

namespace wan::core
{
    std::wstring ThreadKeyFor(std::wstring_view address)
    {
        const auto t = TrimView(address);
        std::wstring digits;
        bool plus = false;
        bool numeric = !t.empty();
        for (size_t i = 0; i < t.size() && numeric; ++i)
        {
            const wchar_t c = t[i];
            if (c >= L'0' && c <= L'9') digits.push_back(c);
            else if (c == L'+' && i == 0) plus = true;
            else if (c == L' ' || c == L'-' || c == L'(' || c == L')' || c == L'.') continue;
            else numeric = false;
        }
        if (!numeric || digits.empty())
        {
            return std::wstring(t);
        }
        if (plus)
        {
            return L"+" + digits;
        }
        if (digits.size() == 11 && digits.starts_with(L"66"))
        {
            return L"+" + digits;
        }
        if ((digits.size() == 9 || digits.size() == 10) && digits[0] == L'0')
        {
            return L"+66" + digits.substr(1);
        }
        return digits;
    }

    bool IsPhoneNumberKey(std::wstring_view key)
    {
        size_t i = (!key.empty() && key[0] == L'+') ? 1 : 0;
        if (i >= key.size())
        {
            return false;
        }
        for (; i < key.size(); ++i)
        {
            if (key[i] < L'0' || key[i] > L'9') return false;
        }
        return true;
    }
}
