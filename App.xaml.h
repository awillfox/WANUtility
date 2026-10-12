#pragma once

#include "App.xaml.g.h"

namespace winrt::WANUtility::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

        void ShowMainWindow();
        void ShowThread(std::wstring const& threadKey);
        void ExitApp();

    private:
        void HandleActivation(Microsoft::Windows::AppLifecycle::AppActivationArguments const& args, bool fromRedirect);

        WANUtility::MainWindow m_window{ nullptr };
    };
}
