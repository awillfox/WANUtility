// SPIKE: throwaway feasibility probe. Answers: can a sideloaded packaged app
// use Windows.Devices.Sms and UssdSession on this machine's EM120R-GL?
#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <fstream>
#include <filesystem>
#include <winrt/Windows.Devices.Sms.h>
#include <winrt/Windows.Networking.NetworkOperators.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.ApplicationModel.Chat.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Windows::Devices::Sms;
using namespace Windows::Networking::NetworkOperators;

namespace
{
    constexpr wchar_t const* kUssdCodes[] = { L"*101#" };
    constexpr wchar_t kSmsTo[] = L"0889605613";
    constexpr wchar_t kSmsBody[] = L"WANUtility probe test SMS";

    std::wstring DecodeUcs2Be(com_array<uint8_t> const& raw)
    {
        std::wstring out;
        for (uint32_t i = 0; i + 1 < raw.size(); i += 2)
            out.push_back(static_cast<wchar_t>((raw[i] << 8) | raw[i + 1]));
        return out;
    }

    std::wstring Ts(Windows::Foundation::DateTime t)
    {
        auto tt = clock::to_time_t(t);
        tm local{};
        localtime_s(&local, &tt);
        wchar_t buf[32];
        wcsftime(buf, 32, L"%Y-%m-%d %H:%M:%S", &local);
        return buf;
    }

    std::wstring Hr(hresult_error const& e)
    {
        wchar_t buf[16];
        swprintf_s(buf, L"0x%08X", static_cast<uint32_t>(e.code().value));
        return std::wstring(buf) + L" " + std::wstring(e.message());
    }
}

namespace winrt::WANUtility::implementation
{
    int32_t MainWindow::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainWindow::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void MainWindow::Log(std::wstring const& line)
    {
        logText().Text(logText().Text() + line + L"\n");
        // UTF-8: wofstream in the C locale silently fails on Thai text.
        std::ofstream f(m_logPath, std::ios::app | std::ios::binary);
        f << to_string(line) << "\n";
    }

    fire_and_forget MainWindow::root_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_started) co_return;
        m_started = true;
        auto strong = get_strong();

        m_logPath = std::wstring(Windows::Storage::ApplicationData::Current().LocalFolder().Path()) + L"\\probe.log";
        Log(L"=== probe start ===");
        co_await DumpStores(L"before USSD");
        co_await SubscribeChat();
        co_await ProbeUssd();
        Log(L"[RECV] waiting up to 120s for incoming SMS (triggered by *101*1#)");
        for (int i = 0; i < 120 && m_received == 0; ++i)
        {
            co_await std::chrono::seconds(1);
            co_await wil::resume_foreground(DispatcherQueue());
        }
        Log(L"[RECV] messages received = " + std::to_wstring(m_received));
        co_await DumpStores(L"after wait");
        co_await ProbeSms();
        Log(L"=== probe end ===");
    }

    Windows::Foundation::IAsyncAction MainWindow::ProbeUssd()
    {
        auto strong = get_strong();
        Log(L"[USSD] enumerating network account ids");
        try
        {
            auto ids = MobileBroadbandAccount::AvailableNetworkAccountIds();
            Log(L"[USSD] account count = " + std::to_wstring(ids.Size()));
            for (auto const& id : ids)
            {
                Log(L"[USSD] account id = " + std::wstring(id));
                try
                {
                    auto acct = MobileBroadbandAccount::CreateFromNetworkAccountId(id);
                    Log(L"[USSD]   provider = " + std::wstring(acct.ServiceProviderName()));
                }
                catch (hresult_error const& e) { Log(L"[USSD]   CreateFromNetworkAccountId failed: " + Hr(e)); }

                for (auto code : kUssdCodes)
                {
                    try
                    {
                        auto session = UssdSession::CreateFromNetworkAccountId(id);
                        Log(L"[USSD]   session created, sending " + std::wstring(code));
                        UssdMessage msg{ code };
                        auto reply = co_await session.SendMessageAndGetReplyAsync(msg);
                        Log(L"[USSD]   ResultCode = " + std::to_wstring(static_cast<int32_t>(reply.ResultCode())));
                        if (auto m = reply.Message())
                        {
                            Log(L"[USSD]   DCS = " + std::to_wstring(m.DataCodingScheme()));
                            Log(L"[USSD]   PayloadAsText = " + std::wstring(m.PayloadAsText()));
                            auto raw = m.GetPayload();
                            std::wstring hex;
                            for (auto b : raw) { wchar_t h[4]; swprintf_s(h, L"%02X ", b); hex += h; }
                            Log(L"[USSD]   raw (" + std::to_wstring(raw.size()) + L" bytes) = " + hex);
                            Log(L"[USSD]   UCS-2 BE decode = " + DecodeUcs2Be(raw));
                        }
                        else
                        {
                            Log(L"[USSD]   reply message = null");
                        }
                        session.Close();
                    }
                    catch (hresult_error const& e) { Log(L"[USSD]   session failed: " + Hr(e)); }
                }
            }
        }
        catch (hresult_error const& e) { Log(L"[USSD] AvailableNetworkAccountIds failed: " + Hr(e)); }
    }

    Windows::Foundation::IAsyncAction MainWindow::SubscribeChat()
    {
        auto strong = get_strong();
        try
        {
            m_chatStore = co_await Windows::ApplicationModel::Chat::ChatMessageManager::RequestStoreAsync();
            m_chatStore.MessageChanged([weak = get_weak()](auto const& store, Windows::ApplicationModel::Chat::ChatMessageChangedEventArgs const&)
            {
                auto self = weak.get();
                if (!self) return;
                // Event carries no details in this SDK: re-read newest message.
                [](auto self, auto store) -> fire_and_forget
                {
                    std::wstring line = L"[LIVE] MessageChanged";
                    try
                    {
                        auto batch = co_await store.GetMessageReader().ReadBatchAsync();
                        if (batch.Size())
                        {
                            auto m = batch.GetAt(0);
                            line += L" newest: " + Ts(m.LocalTimestamp()) + L" from=" + std::wstring(m.From()) + L" body=" + std::wstring(m.Body());
                        }
                    }
                    catch (hresult_error const& ex) { line += L" read failed: " + Hr(ex); }
                    self->DispatcherQueue().TryEnqueue([self, line] { ++self->m_received; self->Log(line); });
                }(self, store);
            });
            Log(L"[LIVE] subscribed to ChatMessageStore.MessageChanged");
        }
        catch (hresult_error const& e) { Log(L"[LIVE] subscribe failed: " + Hr(e)); }
    }

    Windows::Foundation::IAsyncAction MainWindow::DumpStores(std::wstring label)
    {
        auto strong = get_strong();
        Log(L"[STORE] --- " + label + L" ---");
        try
        {
            auto chat = co_await Windows::ApplicationModel::Chat::ChatMessageManager::RequestStoreAsync();
            if (!chat) { Log(L"[STORE] chat store = null"); co_return; }
            auto batch = co_await chat.GetMessageReader().ReadBatchAsync();
            Log(L"[STORE] chat batch count = " + std::to_wstring(batch.Size()));
            for (auto const& m : batch)
            {
                Log(L"[STORE]   " + Ts(m.LocalTimestamp()) + (m.IsIncoming() ? L" IN  from=" : L" OUT to=")
                    + std::wstring(m.IsIncoming() ? m.From() : (m.Recipients().Size() ? m.Recipients().GetAt(0) : hstring{}))
                    + L" kind=" + std::to_wstring(static_cast<int32_t>(m.MessageKind()))
                    + L" body=" + std::wstring(m.Body()));
            }
        }
        catch (hresult_error const& e) { Log(L"[STORE] chat store failed: " + Hr(e)); }
    }

    Windows::Foundation::IAsyncAction MainWindow::ProbeSms()
    {
        auto strong = get_strong();
        Log(L"[SMS] SmsDevice2::GetDefault");
        SmsDevice2 device{ nullptr };
        try
        {
            device = SmsDevice2::GetDefault();
        }
        catch (hresult_error const& e) { Log(L"[SMS] GetDefault failed: " + Hr(e)); co_return; }
        if (!device) { Log(L"[SMS] GetDefault returned null"); co_return; }

        try
        {
            Log(L"[SMS] DeviceId = " + std::wstring(device.DeviceId()));
            Log(L"[SMS] DeviceStatus = " + std::to_wstring(static_cast<int32_t>(device.DeviceStatus())));
            Log(L"[SMS] AccountPhoneNumber = " + std::wstring(device.AccountPhoneNumber()));
            Log(L"[SMS] SmscAddress = " + std::wstring(device.SmscAddress()));
        }
        catch (hresult_error const& e) { Log(L"[SMS] reading properties failed: " + Hr(e)); }

        // One-shot guard: a real SMS costs money, never send twice across relaunches.
        auto marker = std::wstring(Windows::Storage::ApplicationData::Current().LocalFolder().Path()) + L"\\sms_attempted.marker";
        if (std::filesystem::exists(marker))
        {
            Log(L"[SMS] send skipped: marker exists (already attempted once)");
            co_return;
        }
        { std::wofstream m(marker); m << L"attempted"; }

        try
        {
            SmsTextMessage2 msg;
            msg.To(kSmsTo);
            msg.Body(kSmsBody);
            Log(L"[SMS] sending to " + std::wstring(kSmsTo));
            auto result = co_await device.SendMessageAndGetResultAsync(msg);
            Log(L"[SMS] IsSuccessful = " + std::wstring(result.IsSuccessful() ? L"true" : L"false"));
            Log(L"[SMS] ModemErrorCode = " + std::to_wstring(static_cast<int32_t>(result.ModemErrorCode())));
            Log(L"[SMS] NetworkCauseCode = " + std::to_wstring(result.NetworkCauseCode()));
            Log(L"[SMS] TransportFailureCause = " + std::to_wstring(result.TransportFailureCause()));
            Log(L"[SMS] IsErrorTransient = " + std::wstring(result.IsErrorTransient() ? L"true" : L"false"));
        }
        catch (hresult_error const& e) { Log(L"[SMS] send failed: " + Hr(e)); }
    }
}
