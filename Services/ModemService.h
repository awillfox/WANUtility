#pragma once
#include "Services/Signal.h"

#include <string>

namespace wan
{
    struct ModemStatus
    {
        bool ready = false;
        std::wstring carrier;  // empty when unknown
        int signalBars = -1;   // 0–5, -1 when unknown (e.g. no active cellular connection)
        bool wwanInternet = false; // Windows' internet connection is the cellular one
        std::wstring problem;  // user-facing reason when !ready
    };

    struct ModemInfo
    {
        std::wstring model;
        std::wstring firmware;
        std::wstring carrier;
        std::wstring smsc;
    };

    class ModemService
    {
    public:
        void Initialize();
        void Refresh();

        ModemStatus const& Status() const { return m_status; }
        ModemInfo Info() const;
        winrt::Windows::Devices::Sms::SmsDevice2 SmsDevice() const { return m_sms; }
        winrt::hstring NetworkAccountId() const { return m_accountId; }

        Signal<> StatusChanged;

    private:
        winrt::Windows::Devices::Sms::SmsDevice2 m_sms{ nullptr };
        winrt::hstring m_accountId;
        ModemStatus m_status;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
    };
}
