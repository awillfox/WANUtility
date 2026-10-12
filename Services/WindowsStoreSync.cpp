#include "pch.h"
#include "Services/WindowsStoreSync.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/TimeUtil.h"
#include "Core/Log.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Chat;

namespace wan
{
    namespace
    {
        // E_ACCESSDENIED, and NTSTATUS STATUS_ACCESS_DENIED as returned by SmsMessageRegistration in the spike.
        bool IsPermanentDenial(hresult code)
        {
            return code == E_ACCESSDENIED || static_cast<uint32_t>(code.value) == 0xD0000022u;
        }
    }

    bool WindowsStoreSync::Enabled() const
    {
        auto& s = Services();
        return s.db && s.db->GetSetting(L"store_sync_enabled").value_or(L"1") != L"0";
    }

    std::wstring WindowsStoreSync::DisabledReason() const
    {
        auto& s = Services();
        return s.db ? s.db->GetSetting(L"store_sync_disabled_reason").value_or(L"") : std::wstring();
    }

    winrt::fire_and_forget WindowsStoreSync::Start()
    {
        m_timer = Services().dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::minutes(5));
        m_timer.Tick([](auto const&, auto const&) { Services().storeSync->SyncPending(); });
        m_timer.Start();
        co_await SyncAsync();
    }

    winrt::fire_and_forget WindowsStoreSync::SyncPending()
    {
        co_await SyncAsync();
    }

    Windows::Foundation::IAsyncAction WindowsStoreSync::SyncAsync()
    {
        auto& s = Services();
        if (m_running || !Enabled())
        {
            co_return;
        }
        m_running = true;
        try
        {
            auto pending = s.db->PendingSync();
            if (!pending.empty())
            {
                hstring transportId;
                for (auto const& transport : co_await ChatMessageManager::GetTransportsAsync())
                {
                    if (transport.TransportKind() == ChatMessageTransportKind::Text)
                    {
                        transportId = transport.TransportId();
                        break;
                    }
                }
                if (transportId.empty())
                {
                    s.db->DisableSync(L"Windows has no SMS transport in its message store.");
                    core::Log::Write(L"Store sync disabled: no SMS transport");
                }
                else
                {
                    auto store = co_await ChatMessageManager::RequestStoreAsync();
                    for (auto const& m : pending)
                    {
                        ChatMessage cm;
                        cm.TransportId(transportId);
                        cm.Body(m.body);
                        cm.Recipients().Append(m.address);
                        cm.IsIncoming(false);
                        cm.IsRead(true);
                        cm.MessageOperatorKind(ChatMessageOperatorKind::Sms);
                        cm.Status(ChatMessageStatus::Sent);
                        cm.LocalTimestamp(FromUnixMs(m.timestampMs));
                        cm.NetworkTimestamp(FromUnixMs(m.timestampMs));
                        co_await store.SaveMessageAsync(cm);
                        const std::wstring storeId{ cm.Id() };
                        s.db->MarkSynced(m.id, storeId);
                        core::Log::Write(L"Synced message " + std::to_wstring(m.id) + L" to Windows store as '" + storeId + L"'");
                    }
                }
            }
        }
        catch (hresult_error const& e)
        {
            if (IsPermanentDenial(e.code()))
            {
                try
                {
                    s.db->DisableSync(L"Windows denied access to its message store (" + DescribeError(e) + L").");
                }
                catch (core::DbError const& dbError)
                {
                    ReportError(L"Saving sync state failed: " + Widen(dbError.what()));
                }
                core::Log::Write(L"Store sync disabled: " + DescribeError(e));
            }
            else
            {
                core::Log::Write(L"Store sync will retry later: " + DescribeError(e));
            }
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Store sync failed: " + Widen(e.what()));
        }
        m_running = false;
        StateChanged.Raise();
    }
}
