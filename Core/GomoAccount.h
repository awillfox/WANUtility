#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace wan::core
{
    enum class GomoFailure { Network, NotCellular, NotRecognized, Blocked, SessionExpired, Service, InvalidResponse };

    struct GomoError : std::runtime_error
    {
        explicit GomoError(GomoFailure reason) : std::runtime_error("GOMO request failed"), reason(reason) {}
        GomoFailure reason;
    };

    struct GomoBalance
    {
        std::wstring remaining;
        std::wstring maximum;
    };

    struct GomoPackage
    {
        std::wstring name, description, status, startDate, endDate, allowance, speed;
        std::vector<std::wstring> remaining;
    };

    struct GomoAccount
    {
        GomoBalance data, calls;
        std::vector<GomoPackage> packages;
    };

    void CheckGomoResponse(std::wstring_view json);
    std::wstring ParseGomoLogin(std::wstring_view json);
    GomoAccount ParseGomoAccount(std::wstring_view usage, std::wstring_view packages);

    // GOMO is an AIS brand. True when the registered provider name's first word is "AIS" (observed: "AIS").
    bool IsAisCarrier(std::wstring_view registeredProviderName);

    // Carrier dates are UTC ISO 8601 ("2026-10-20T11:09:15.350Z"); returns local "20/10/2026 18:09".
    // Anything unparsable or outside the local-time range is returned as sent.
    std::wstring FormatGomoDate(std::wstring const& iso);
}
