#include "pch.h"
#include "Services/ModemService.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Core/Log.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Sms;
using namespace winrt::Windows::Networking::Connectivity;
using namespace winrt::Windows::Networking::NetworkOperators;

namespace wan
{
    namespace
    {
        std::wstring DescribeDeviceStatus(SmsDeviceStatus status)
        {
            switch (status)
            {
            case SmsDeviceStatus::Off: return L"The cellular radio is off. Check airplane mode.";
            case SmsDeviceStatus::SimNotInserted: return L"No SIM card is inserted.";
            case SmsDeviceStatus::BadSim: return L"The SIM card is not working.";
            case SmsDeviceStatus::DeviceFailure: return L"The modem reported a failure.";
            case SmsDeviceStatus::SubscriptionNotActivated: return L"The SIM subscription is not activated.";
            case SmsDeviceStatus::DeviceLocked: return L"The SIM is locked (PIN required).";
            case SmsDeviceStatus::DeviceBlocked: return L"The SIM is blocked (PUK required).";
            default: return L"The modem is not ready.";
            }
        }
    }

    void ModemService::Initialize()
    {
        try
        {
            m_sms = SmsDevice2::GetDefault();
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"SmsDevice2::GetDefault failed: " + DescribeError(e));
        }
        try
        {
            auto ids = MobileBroadbandAccount::AvailableNetworkAccountIds();
            if (ids.Size() > 0)
            {
                m_accountId = ids.GetAt(0);
            }
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"AvailableNetworkAccountIds failed: " + DescribeError(e));
        }

        // Both events fire on worker threads: hop to the UI thread before touching state.
        if (m_sms)
        {
            m_sms.DeviceStatusChanged([](SmsDevice2 const&, winrt::Windows::Foundation::IInspectable const&)
            {
                Services().dispatcher.TryEnqueue([] { Services().modem->Refresh(); });
            });
        }
        NetworkInformation::NetworkStatusChanged([](winrt::Windows::Foundation::IInspectable const&)
        {
            Services().dispatcher.TryEnqueue([] { Services().modem->Refresh(); });
        });

        // Signal strength has no change event; poll it.
        m_timer = Services().dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::seconds(30));
        m_timer.Tick([](auto const&, auto const&) { Services().modem->Refresh(); });
        m_timer.Start();

        Refresh();
    }

    void ModemService::Refresh()
    {
        ModemStatus s;
        if (!m_sms)
        {
            s.problem = L"No cellular modem was found.";
        }
        else
        {
            try
            {
                const auto status = m_sms.DeviceStatus();
                s.ready = status == SmsDeviceStatus::Ready;
                if (!s.ready)
                {
                    s.problem = DescribeDeviceStatus(status);
                }
            }
            catch (hresult_error const& e)
            {
                s.problem = L"Cannot read the modem status: " + DescribeError(e);
            }
        }

        if (!m_accountId.empty())
        {
            try
            {
                auto account = MobileBroadbandAccount::CreateFromNetworkAccountId(m_accountId);
                if (auto network = account.CurrentNetwork())
                {
                    s.carrier = std::wstring(network.RegisteredProviderName());
                }
            }
            catch (hresult_error const& e)
            {
                core::Log::Write(L"Reading carrier failed: " + DescribeError(e));
            }
        }

        try
        {
            for (auto const& profile : NetworkInformation::GetConnectionProfiles())
            {
                if (!profile.IsWwanConnectionProfile())
                {
                    continue;
                }
                if (auto bars = profile.GetSignalBars())
                {
                    s.signalBars = bars.Value();
                    break;
                }
            }
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Reading signal failed: " + DescribeError(e));
        }

        try
        {
            auto internet = NetworkInformation::GetInternetConnectionProfile();
            s.wwanInternet = internet && internet.IsWwanConnectionProfile();
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Reading internet connection failed: " + DescribeError(e));
        }

        const bool changed = s.ready != m_status.ready || s.carrier != m_status.carrier ||
                             s.signalBars != m_status.signalBars || s.problem != m_status.problem ||
                             s.wwanInternet != m_status.wwanInternet;
        m_status = s;
        if (changed)
        {
            core::Log::Write(L"Modem: ready=" + std::to_wstring(s.ready) + L" carrier='" + s.carrier +
                             L"' bars=" + std::to_wstring(s.signalBars) + L" wwanInternet=" + std::to_wstring(s.wwanInternet) +
                             L" problem='" + s.problem + L"'");
            StatusChanged.Raise();
        }
    }

    ModemInfo ModemService::Info() const
    {
        ModemInfo info;
        info.carrier = m_status.carrier;
        try
        {
            if (m_sms) info.smsc = std::wstring(m_sms.SmscAddress());
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Reading SMSC failed: " + DescribeError(e));
        }
        try
        {
            if (!m_accountId.empty())
            {
                auto device = MobileBroadbandAccount::CreateFromNetworkAccountId(m_accountId).CurrentDeviceInformation();
                info.model = std::wstring(device.Model());
                info.firmware = std::wstring(device.FirmwareInformation());
            }
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Reading device info failed: " + DescribeError(e));
        }
        return info;
    }
}
