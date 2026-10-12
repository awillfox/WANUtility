#pragma once

#include "SettingsPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage() {}

        void Page_Loaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void Page_Unloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget StartupToggle_Toggled(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        fire_and_forget LoadStartup();
        void ApplyStartupState(Windows::ApplicationModel::StartupTaskState state);
        void RenderSync();
        void RenderCodes();
        void RenderModem();

        bool m_suppressToggle = false;
        int m_syncToken = 0;
        int m_modemToken = 0;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
