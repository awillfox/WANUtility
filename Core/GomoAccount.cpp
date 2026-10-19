#include "GomoAccount.h"
#include "TextUtil.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cwctype>
#include <sstream>
#include <utility>

namespace wan::core
{
    namespace
    {
        using namespace winrt::Windows::Data::Json;

        JsonObject Response(std::wstring_view json)
        {
            JsonObject result;
            if (!JsonObject::TryParse(winrt::hstring(json), result)) throw GomoError(GomoFailure::InvalidResponse);
            auto status = result.GetNamedNumber(L"statusCode", 0);
            if (status == 401 || status == 403) throw GomoError(GomoFailure::SessionExpired);
            if (status != 200) throw GomoError(GomoFailure::Service);
            return result;
        }

        // Optional fields: a missing key, null or another type gives an empty object/array. The platform's
        // GetNamedObject(name, default) throws when the key exists with another type, e.g. null.
        JsonObject OptionalObject(IJsonValue const& value)
        {
            return value && value.ValueType() == JsonValueType::Object ? value.GetObject() : nullptr;
        }

        JsonObject OptionalObject(JsonObject const& object, wchar_t const* name)
        {
            return object ? OptionalObject(object.GetNamedValue(name, nullptr)) : nullptr;
        }

        JsonArray OptionalArray(JsonObject const& object, wchar_t const* name)
        {
            auto value = object ? object.GetNamedValue(name, nullptr) : nullptr;
            return value && value.ValueType() == JsonValueType::Array ? value.GetArray() : nullptr;
        }

        std::wstring Scalar(JsonObject const& object, wchar_t const* name)
        {
            if (!object) return {};
            auto value = object.GetNamedValue(name, nullptr);
            if (!value) return {};
            std::wstring text;
            if (value.ValueType() == JsonValueType::String) text = value.GetString();
            else if (value.ValueType() == JsonValueType::Number) text = value.Stringify();
            text = Trim(text);
            return text == L"undefined" || text == L"null" ? std::wstring{} : text;
        }

        std::wstring Localized(JsonObject const& object)
        {
            auto en = Scalar(object, L"en");
            return en.empty() ? Scalar(object, L"th") : en;
        }

        // Carrier text uses simple inline markup (observed: b, big, br, font, span). Every tag becomes a space and
        // character references are decoded, so the result is inert plain text for a native TextBlock.
        // Windows.Data.Html::HtmlUtilities::ConvertToText is not used: it fail-fasts in the test host.
        std::wstring Plain(std::wstring const& html)
        {
            static constexpr std::pair<wchar_t const*, wchar_t> named[] = {
                { L"amp", L'&' }, { L"lt", L'<' }, { L"gt", L'>' }, { L"quot", L'"' }, { L"apos", L'\'' }, { L"nbsp", L' ' } };
            std::wstring result;
            auto append = [&](wchar_t c)
            {
                if (std::iswspace(c) || c == 0xA0)
                {
                    if (!result.empty() && result.back() != L' ') result += L' ';
                }
                else if (c >= 0x20 && c != 0x7F) result += c;
            };
            for (size_t i = 0; i < html.size(); ++i)
            {
                wchar_t c = html[i];
                if (c == L'<' && i + 1 < html.size() && (std::iswalpha(html[i + 1]) || html[i + 1] == L'/'))
                {
                    if (auto close = html.find(L'>', i); close != std::wstring::npos)
                    {
                        append(L' ');
                        i = close;
                        continue;
                    }
                }
                if (c == L'&')
                {
                    auto semi = html.find(L';', i);
                    if (semi != std::wstring::npos && semi - i <= 10)
                    {
                        auto name = html.substr(i + 1, semi - i - 1);
                        unsigned long code = 0;
                        if (name.size() > 1 && name[0] == L'#')
                        {
                            bool hex = name[1] == L'x' || name[1] == L'X';
                            auto digits = name.substr(hex ? 2 : 1);
                            if (!digits.empty() && std::all_of(digits.begin(), digits.end(), [hex](wchar_t d) { return hex ? std::iswxdigit(d) : std::iswdigit(d); }))
                                code = std::wcstoul(digits.c_str(), nullptr, hex ? 16 : 10);
                        }
                        else
                        {
                            for (auto const& [entity, value] : named)
                                if (name == entity) code = value;
                        }
                        if (code > 0 && code <= 0xFFFF && (code < 0xD800 || code > 0xDFFF))
                        {
                            append(static_cast<wchar_t>(code));
                            i = semi;
                            continue;
                        }
                    }
                }
                append(c);
            }
            return Trim(result);
        }

        std::wstring Quantity(JsonObject const& object, wchar_t const* valueName = L"value")
        {
            auto value = Scalar(object, valueName);
            auto unit = Scalar(object, L"unit");
            return value.empty() || unit.empty() ? std::wstring{} : value + L" " + unit;
        }

        GomoBalance Balance(JsonObject const& data, wchar_t const* name)
        {
            auto balance = data.GetNamedObject(name);
            auto remaining = Scalar(balance, L"remaining");
            auto unit = Localized(balance.GetNamedObject(L"unit"));
            if (remaining.empty() || unit.empty()) throw GomoError(GomoFailure::InvalidResponse);
            auto maximum = Scalar(balance, L"max");
            return { Plain(remaining + L" " + unit), maximum.empty() ? L"" : Plain(maximum + L" " + unit) };
        }

        void Remaining(std::vector<std::wstring>& lines, JsonObject const& freeUnit, wchar_t const* key, wchar_t const* label)
        {
            if (!freeUnit) return;
            auto entries = OptionalArray(freeUnit, key);
            if (!entries) return;
            for (auto const& entry : entries)
            {
                auto item = OptionalObject(entry);
                auto text = Localized(OptionalObject(item, L"android"));
                if (text.empty()) text = Localized(OptionalObject(item, L"ios"));
                text = Plain(text);
                if (!text.empty()) lines.push_back(std::wstring(label) + text);
            }
        }

        std::wstring PackageStatus(JsonObject const& item)
        {
            static constexpr wchar_t const* names[] = { L"Active", L"Inactive", L"Expired", L"Temporary", L"Default", L"Pending", L"Waiting", L"Used" };
            auto status = item.GetNamedNumber(L"statusId", -1);
            if (status >= 0 && status < std::size(names) && status == static_cast<int>(status))
                return names[static_cast<int>(status)];
            return L"Status unavailable";
        }
    }

    void CheckGomoResponse(std::wstring_view json)
    {
        try { Response(json); }
        catch (winrt::hresult_error const&) { throw GomoError(GomoFailure::InvalidResponse); }
    }

    std::wstring ParseGomoLogin(std::wstring_view json)
    {
        try
        {
            auto result = Response(json);
            if (result.GetNamedValue(L"myAISBlocked", JsonValue::CreateNullValue()).ValueType() != JsonValueType::Null)
                throw GomoError(GomoFailure::Blocked);
            if (result.GetNamedBoolean(L"shouldRedirect", false) || result.GetNamedBoolean(L"shouldOpenWithDeviceBrowser", false))
                throw GomoError(GomoFailure::NotRecognized);
            auto token = Scalar(result, L"rocketToken");
            if (token.empty() || token.size() > 16384 || !std::all_of(token.begin(), token.end(), [](wchar_t c) { return c >= 0x21 && c <= 0x7E; }))
                throw GomoError(GomoFailure::NotRecognized);
            return token;
        }
        catch (winrt::hresult_error const&) { throw GomoError(GomoFailure::InvalidResponse); }
    }

    bool IsAisCarrier(std::wstring_view registeredProviderName)
    {
        auto name = TrimView(registeredProviderName);
        size_t end = 0;
        while (end < name.size() && std::iswalpha(name[end])) ++end;
        auto word = name.substr(0, end);
        return word.size() == 3 && std::towupper(word[0]) == L'A' && std::towupper(word[1]) == L'I' && std::towupper(word[2]) == L'S';
    }

    std::wstring FormatGomoDate(std::wstring const& iso)
    {
        std::wistringstream in(iso);
        std::chrono::sys_time<std::chrono::milliseconds> t;
        in >> std::chrono::parse(L"%FT%TZ", t);
        if (in.fail()) return iso;
        const time_t secs = std::chrono::system_clock::to_time_t(t);
        tm local{};
        // localtime_s rejects times before 1970 (and far-future ones); wcsftime would then end the process.
        if (localtime_s(&local, &secs) != 0) return iso;
        wchar_t buf[32];
        return wcsftime(buf, 32, L"%d/%m/%Y %H:%M", &local) ? std::wstring(buf) : iso;
    }

    GomoAccount ParseGomoAccount(std::wstring_view usage, std::wstring_view packages)
    {
        try
        {
            auto data = Response(usage).GetNamedObject(L"data");
            GomoAccount account;
            account.data = Balance(data, L"displayInternet");
            account.calls = Balance(data, L"displayVoice");
            auto items = Response(packages).GetNamedObject(L"data").GetNamedArray(L"packages");
            for (auto const& value : items)
            {
                auto item = value.GetObject();
                auto info = item.GetNamedObject(L"packageId");
                GomoPackage p;
                p.name = Plain(Localized(info.GetNamedObject(L"displayName")));
                if (p.name.empty()) throw GomoError(GomoFailure::InvalidResponse);
                p.description = Localized(OptionalObject(info, L"detail"));
                if (p.description.empty()) p.description = Localized(OptionalObject(info, L"smsName"));
                p.description = Plain(p.description);
                p.status = PackageStatus(item);
                p.startDate = Scalar(item, L"startDate");
                p.endDate = Scalar(item, L"endDate");
                p.allowance = Plain(Quantity(OptionalObject(info, L"dataVolume")));
                p.speed = Plain(Quantity(OptionalObject(info, L"speed")));
                auto freeUnit = OptionalObject(info, L"remainingFreeUnit");
                Remaining(p.remaining, freeUnit, L"detailInternet", L"Data: ");
                if (p.remaining.empty())
                {
                    auto balance = Quantity(OptionalObject(info, L"dataBalance"), L"remain");
                    if (!balance.empty()) p.remaining.push_back(L"Data: " + Plain(balance) + L" remaining");
                }
                Remaining(p.remaining, freeUnit, L"detailVoice", L"Calls: ");
                account.packages.push_back(std::move(p));
            }
            return account;
        }
        catch (winrt::hresult_error const&) { throw GomoError(GomoFailure::InvalidResponse); }
    }
}
