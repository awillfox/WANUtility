#include "pch.h"
#include "Services/GomoClient.h"
#include "Services/ErrorText.h"
#include "Core/Log.h"

#include <winhttp.h>

using namespace winrt;
using namespace winrt::Windows::Networking::Connectivity;
using wan::core::GomoError;
using wan::core::GomoFailure;

namespace wan
{
    namespace
    {
        constexpr wchar_t kHost[] = L"gomo.th";
        constexpr size_t kMaxBody = 1024 * 1024;
        constexpr int kTimeoutMs = 15000;

        // Headers of the GOMO Android app (version 244), which this flow was verified with.
        constexpr wchar_t kHeaders[] =
            L"language: en\r\n"
            L"network: GOMO\r\n"
            L"Connection: close\r\n"
            L"channel: ANDROID\r\n"
            L"version: 244\r\n"
            L"Content-Type: application/json;charset=utf-8\r\n";

        using Handle = std::unique_ptr<void, decltype(&WinHttpCloseHandle)>;

        Handle Check(HINTERNET h)
        {
            if (!h) throw GomoError(GomoFailure::Network);
            return Handle(h, &WinHttpCloseHandle);
        }

        // One GET. Redirects, proxies, cookies and automatic credentials are disabled; HTTPS certificates are
        // validated by WinHTTP's defaults. Only HTTP 200 is accepted. The token is never logged.
        std::wstring Get(HINTERNET session, bool secure, wchar_t const* path, std::wstring const& authorization)
        {
            auto connection = Check(WinHttpConnect(session, kHost, secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0));
            auto request = Check(WinHttpOpenRequest(connection.get(), L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                    WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
            DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
            DWORD autologon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
            if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)) ||
                !WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)) ||
                !WinHttpSetOption(request.get(), WINHTTP_OPTION_AUTOLOGON_POLICY, &autologon, sizeof(autologon)))
                throw GomoError(GomoFailure::Network);

            std::wstring headers = kHeaders;
            headers += L"authorization: " + authorization + L"\r\n";
            if (!WinHttpSendRequest(request.get(), headers.c_str(), static_cast<DWORD>(headers.size()), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                !WinHttpReceiveResponse(request.get(), nullptr))
            {
                core::Log::Write(L"GOMO " + std::wstring(path) + L" transport error " + std::to_wstring(GetLastError()));
                throw GomoError(GomoFailure::Network);
            }

            DWORD status = 0;
            DWORD size = sizeof(status);
            if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
                throw GomoError(GomoFailure::Network);
            core::Log::Write(L"GOMO " + std::wstring(path) + L" HTTP " + std::to_wstring(status));
            if (status >= 300 && status < 400) throw GomoError(GomoFailure::NotRecognized); // e.g. sent to a login page
            if (status == 401 || status == 403) throw GomoError(GomoFailure::SessionExpired);
            if (status != 200) throw GomoError(GomoFailure::Service);

            std::string body;
            for (;;)
            {
                char buffer[8192];
                DWORD read = 0;
                if (!WinHttpReadData(request.get(), buffer, sizeof(buffer), &read)) throw GomoError(GomoFailure::Network);
                if (read == 0) break;
                if (body.size() + read > kMaxBody) throw GomoError(GomoFailure::InvalidResponse);
                body.append(buffer, read);
            }
            return Widen(body);
        }
    }

    core::GomoAccount FetchGomoAccount()
    {
        // The Android app also requires cellular transport first; over Wi-Fi or a VPN the carrier cannot
        // identify the subscriber, and the request would leave through another network.
        try
        {
            auto profile = NetworkInformation::GetInternetConnectionProfile();
            if (!profile || !profile.IsWwanConnectionProfile()) throw GomoError(GomoFailure::NotCellular);
        }
        catch (hresult_error const&)
        {
            throw GomoError(GomoFailure::NotCellular);
        }

        auto session = Check(WinHttpOpen(L"okhttp/4.12.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!WinHttpSetTimeouts(session.get(), kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs)) throw GomoError(GomoFailure::Network);

        core::CheckGomoResponse(Get(session.get(), true, L"/api/config/mobile-check", L""));
        // Plain HTTP on purpose: this is the request the carrier identifies. The token it returns is only
        // ever sent over HTTPS.
        const auto token = core::ParseGomoLogin(Get(session.get(), false, L"/kyc/login/msisdn/", L""));
        auto usage = Get(session.get(), true, L"/api/profile/usage", token);
        auto packages = Get(session.get(), true, L"/api/profile/packages?inquiryUsage=true", token);
        return core::ParseGomoAccount(usage, packages);
    }
}
