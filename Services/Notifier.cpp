#include "pch.h"
#include "Services/Notifier.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Core/Log.h"

using namespace winrt;
using namespace winrt::Microsoft::Windows::AppNotifications;
using namespace winrt::Microsoft::Windows::AppNotifications::Builder;

namespace wan
{
    std::wstring Notifier::ThreadFromArguments(Windows::Foundation::Collections::IMap<hstring, hstring> const& arguments)
    {
        return arguments && arguments.HasKey(L"thread") ? std::wstring(arguments.Lookup(L"thread")) : std::wstring();
    }

    void Notifier::Initialize(std::function<void(std::wstring const&)> openThread)
    {
        auto manager = AppNotificationManager::Default();
        // Subscribe before Register(). Raised on a worker thread when a toast is clicked while we run.
        manager.NotificationInvoked([openThread](AppNotificationManager const&, AppNotificationActivatedEventArgs const& args)
        {
            auto key = ThreadFromArguments(args.Arguments());
            Services().dispatcher.TryEnqueue([openThread, key] { openThread(key); });
        });
        try
        {
            manager.Register();
            m_registered = true;
        }
        catch (hresult_error const& e)
        {
            ReportError(L"Notifications are unavailable: " + DescribeError(e));
        }
    }

    void Notifier::ShowIncoming(core::Message const& message)
    {
        auto& s = Services();
        if (!m_registered)
        {
            return;
        }
        if (s.windowFocused && s.threadInView == message.threadKey)
        {
            return; // the user is already looking at this conversation
        }
        const std::wstring body = message.body.size() > 200 ? message.body.substr(0, 200) + L"…" : message.body;
        try
        {
            auto toast = AppNotificationBuilder()
                .AddArgument(L"thread", message.threadKey)
                .AddText(message.address)
                .AddText(body)
                .BuildNotification();
            AppNotificationManager::Default().Show(toast);
            core::Log::Write(L"Toast shown for thread " + message.threadKey);
        }
        catch (hresult_error const& e)
        {
            core::Log::Write(L"Showing a toast failed: " + DescribeError(e));
        }
    }

    void Notifier::Shutdown()
    {
        if (m_registered)
        {
            AppNotificationManager::Default().Unregister();
            m_registered = false;
        }
    }
}
