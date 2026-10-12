#pragma once

#include "MessagesPage.g.h"
#include "Core/MessageDb.h"

namespace winrt::WANUtility::implementation
{
    struct MessagesPage : MessagesPageT<MessagesPage>
    {
        MessagesPage() {}

        void Page_Loaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void Page_Unloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SearchBox_TextChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void ThreadList_SelectionChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void NewButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void ComposeInput_TextChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void SendButton_Click(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

    private:
        void RefreshThreads();
        void RefreshConversation();
        void SelectThread(std::wstring const& threadKey);
        void UpdateSendEnabled();
        Microsoft::UI::Xaml::Controls::ListViewItem MakeThreadRow(wan::core::Thread const& thread);
        Microsoft::UI::Xaml::UIElement MakeBubble(wan::core::Message const& message);
        Microsoft::UI::Xaml::Style LocalStyle(wchar_t const* key);

        std::wstring m_threadKey;     // selected thread, empty when none
        std::wstring m_replyAddress;  // address to send replies to (newest message's address)
        bool m_newMode = false;       // composing to a new recipient
        bool m_refreshing = false;    // suppress SelectionChanged while rebuilding the list
        int m_messagesToken = 0;
        int m_openToken = 0;
        int m_modemToken = 0;
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct MessagesPage : MessagesPageT<MessagesPage, implementation::MessagesPage>
    {
    };
}
