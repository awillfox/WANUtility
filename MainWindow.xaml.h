#pragma once

#include "MainWindow.g.h"
#include <winrt/Windows.Devices.Sms.h>
#include <winrt/Windows.ApplicationModel.Chat.h>

namespace winrt::WANUtility::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }

        int32_t MyProperty();
        void MyProperty(int32_t value);

        fire_and_forget root_Loaded(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void Log(std::wstring const& line);
        Windows::Foundation::IAsyncAction ProbeUssd();
        Windows::Foundation::IAsyncAction ProbeSms();
        Windows::Foundation::IAsyncAction DumpStores(std::wstring label);
        Windows::Foundation::IAsyncAction SubscribeChat();

        Windows::ApplicationModel::Chat::ChatMessageStore m_chatStore{ nullptr };

        int m_received{ 0 };

        std::wstring m_logPath;
        bool m_started{ false };
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
