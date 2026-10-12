#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include "Core/Log.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/InboxImporter.h"
#include "Services/Notifier.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::Windows::AppLifecycle;

namespace
{
    struct ActivationInfo
    {
        ExtendedActivationKind kind = ExtendedActivationKind::Launch;
        std::wstring toastThread;
    };

    // Reads everything HandleActivation needs out of the arguments immediately. A redirected
    // activation's arguments are proxies into the launching process, which exits as soon as our
    // Activated handler returns; calls made later from the UI thread failed with 0x800706BE (observed).
    ActivationInfo ReadActivation(AppActivationArguments const& args)
    {
        ActivationInfo info;
        info.kind = args.Kind();
        if (info.kind == ExtendedActivationKind::AppNotification)
        {
            auto toast = args.Data().as<winrt::Microsoft::Windows::AppNotifications::AppNotificationActivatedEventArgs>();
            info.toastThread = wan::Notifier::ThreadFromArguments(toast.Arguments());
        }
        return info;
    }
}

namespace winrt::WANUtility::implementation
{
    App::App()
    {
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            wan::core::Log::Write(L"UNHANDLED " + std::wstring(e.Message()));
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
#endif
        });
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        wan::InitializeServices();

        auto& services = wan::Services();
        services.tray = std::make_unique<wan::TrayIcon>();
        services.tray->Create(std::wstring(Windows::ApplicationModel::Package::Current().InstalledLocation().Path()) + L"\\Assets\\Tray.ico",
                              L"WANUtility",
                              [this] { ShowMainWindow(); },
                              [this] { ExitApp(); });

        services.notifier = std::make_unique<wan::Notifier>();
        services.notifier->Initialize([this](std::wstring const& threadKey) { ShowThread(threadKey); });
        services.importer->NewIncoming.Add([](wan::core::Message const& message) { wan::Services().notifier->ShowIncoming(message); });

        // [app-init]

        m_window = make<MainWindow>();

        // A second launch (or a toast click) in another process is redirected here by main.cpp.
        AppInstance::GetCurrent().Activated([this](IInspectable const&, AppActivationArguments const& args)
        {
            // Runs on a worker thread while the launching process waits for us to return.
            ActivationInfo info;
            try
            {
                info = ReadActivation(args);
            }
            catch (hresult_error const& e)
            {
                wan::core::Log::Write(L"Reading redirected activation failed: " + wan::DescribeError(e)); // still show the window
            }
            wan::Services().dispatcher.TryEnqueue([this, info] { HandleActivation(info.kind, info.toastThread, true); });
        });
        const auto info = ReadActivation(AppInstance::GetCurrent().GetActivatedEventArgs());
        HandleActivation(info.kind, info.toastThread, false);
    }

    void App::HandleActivation(ExtendedActivationKind kind, std::wstring const& toastThread, bool fromRedirect)
    {
        wan::core::Log::Write(L"Activation kind " + std::to_wstring(static_cast<int>(kind)) + (fromRedirect ? L" (redirected)" : L""));

        if (kind == ExtendedActivationKind::AppNotification)
        {
            // App was launched (or redirected) by clicking a toast after it had exited.
            ShowThread(toastThread);
            return;
        }

        if (kind == ExtendedActivationKind::StartupTask && !fromRedirect)
        {
            wan::core::Log::Write(L"Started at sign-in: staying in the tray");
            return;
        }

        // [activation]

        ShowMainWindow();
    }

    void App::ShowMainWindow()
    {
        get_self<MainWindow>(m_window)->ShowAndFocus();
    }

    void App::ShowThread(std::wstring const& threadKey)
    {
        ShowMainWindow();
        get_self<MainWindow>(m_window)->ShowThread(threadKey);
    }

    void App::ExitApp()
    {
        wan::core::Log::Write(L"Exit requested");

        if (wan::Services().tray) wan::Services().tray->Remove();

        if (wan::Services().notifier) wan::Services().notifier->Shutdown();

        // [exit]

        get_self<MainWindow>(m_window)->AllowCloseAndClose();
        Application::Current().Exit();
    }
}
