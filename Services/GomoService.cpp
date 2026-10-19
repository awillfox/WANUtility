#include "pch.h"
#include "Services/GomoService.h"
#include "Services/AppServices.h"
#include "Services/GomoClient.h"
#include "Services/TimeUtil.h"
#include "Core/Log.h"

using wan::core::GomoFailure;

namespace wan
{
    namespace
    {
        std::wstring Describe(GomoFailure reason)
        {
            switch (reason)
            {
            case GomoFailure::Network: return L"Could not reach GOMO. Check the cellular connection and try again.";
            case GomoFailure::NotCellular: return L"GOMO identifies your number from the cellular connection. Make sure Windows uses Mobile, not Wi-Fi or a VPN, for internet, then refresh.";
            case GomoFailure::NotRecognized: return L"GOMO did not recognize this connection. Make sure the GOMO SIM is connected over cellular data.";
            case GomoFailure::Blocked: return L"GOMO reports that this number is blocked from account access.";
            case GomoFailure::SessionExpired: return L"The GOMO session expired. Refresh to sign in again.";
            case GomoFailure::Service: return L"GOMO returned an error. Try again later.";
            case GomoFailure::InvalidResponse: return L"GOMO sent an incomplete or unexpected response.";
            }
            return L"GOMO request failed.";
        }
    }

    bool GomoService::Available() const
    {
        auto const& modem = Services().modem->Status();
        return modem.wwanInternet && core::IsAisCarrier(modem.carrier);
    }

    winrt::fire_and_forget GomoService::Refresh()
    {
        if (m_state == GomoState::Loading || !Available())
        {
            co_return;
        }
        m_state = GomoState::Loading;
        m_account = {};
        m_error.clear();
        Changed.Raise();

        auto dispatcher = Services().dispatcher;
        core::GomoAccount account;
        std::wstring error;
        co_await winrt::resume_background();
        try
        {
            account = FetchGomoAccount();
        }
        catch (core::GomoError const& e)
        {
            error = Describe(e.reason);
            core::Log::Write(L"GOMO refresh failed (" + std::to_wstring(static_cast<int>(e.reason)) + L")");
        }
        catch (...)
        {
            error = L"GOMO request failed unexpectedly.";
            core::Log::Write(L"GOMO refresh failed (unexpected exception)");
        }
        co_await wil::resume_foreground(dispatcher);

        if (error.empty())
        {
            m_account = std::move(account);
            m_updatedMs = NowMs();
            m_state = GomoState::Loaded;
        }
        else
        {
            m_error = std::move(error);
            m_state = GomoState::Failed;
        }
        Changed.Raise();
    }
}
