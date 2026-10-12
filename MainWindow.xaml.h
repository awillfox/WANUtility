#pragma once

#include "MainWindow.g.h"

namespace winrt::WANUtility::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() {}

        // Runs after XAML loads (C++/WinRT deferred InitializeComponent pattern).
        void InitializeComponent();

        void Nav_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const& sender,
                                  Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

        void ShowAndFocus();
        void ShowThread(std::wstring const& threadKey);
        void AllowCloseAndClose();

    private:
        void OnClosing(Microsoft::UI::Windowing::AppWindow const& sender, Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args);
        void ShowError(std::wstring const& message);
        void UpdateModemUi();

        HWND m_hwnd{};
        bool m_closeToTray = true;
        bool m_allowClose = false;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
