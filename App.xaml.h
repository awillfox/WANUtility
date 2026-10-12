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
        // toastThread is the clicked toast's thread key (AppNotification activations only).
        void HandleActivation(Microsoft::Windows::AppLifecycle::ExtendedActivationKind kind, std::wstring const& toastThread, bool fromRedirect);

        WANUtility::MainWindow m_window{ nullptr };
    };
}
