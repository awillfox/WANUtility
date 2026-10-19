#pragma once

#include "GomoPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct GomoPage : GomoPageT<GomoPage>
    {
        GomoPage() {}

        void Page_Loaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void Page_Unloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void RefreshButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        void Render();
        void RefreshIfFirstVisit();

        int m_gomoToken = 0;
        int m_modemToken = 0;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct GomoPage : GomoPageT<GomoPage, implementation::GomoPage>
    {
    };
}
