#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include "Core/Log.h"
#include "Services/AppServices.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::Windows::AppLifecycle;

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

        // [app-init]

        m_window = make<MainWindow>();

        // A second launch (or a toast click) in another process is redirected here by main.cpp.
        AppInstance::GetCurrent().Activated([this](IInspectable const&, AppActivationArguments const& args)
        {
            wan::Services().dispatcher.TryEnqueue([this, args] { HandleActivation(args, true); });
        });
        HandleActivation(AppInstance::GetCurrent().GetActivatedEventArgs(), false);
    }

    void App::HandleActivation(AppActivationArguments const& args, bool fromRedirect)
    {
        wan::core::Log::Write(L"Activation kind " + std::to_wstring(static_cast<int>(args.Kind())) + (fromRedirect ? L" (redirected)" : L""));

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

        // [exit]

        get_self<MainWindow>(m_window)->AllowCloseAndClose();
        Application::Current().Exit();
    }
}
