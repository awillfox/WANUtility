#pragma once

#include "UssdPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct UssdPage : UssdPageT<UssdPage>
    {
        UssdPage() {}

        void Page_Loaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void Page_Unloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void DialBox_TextChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void DialButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SendReplyButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void EndButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget SaveCodeButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        void Render();
        void RenderChips();
        void RenderRecent();

        int m_ussdToken = 0;
        int m_modemToken = 0;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct UssdPage : UssdPageT<UssdPage, implementation::UssdPage>
    {
    };
}
