#include "pch.h"
#include "MessagesPage.xaml.h"
#if __has_include("MessagesPage.g.cpp")
#include "MessagesPage.g.cpp"
#endif

#include "Core/PhoneNumber.h"
#include "Core/TextUtil.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/SmsSender.h"
#include "Services/TimeUtil.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using wan::core::Direction;
using wan::core::MessageStatus;

namespace winrt::WANUtility::implementation
{
    void MessagesPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        m_messagesToken = s.MessagesChanged.Add([this](std::wstring const&)
        {
            RefreshThreads();
            RefreshConversation();
        });
        m_openToken = s.OpenThreadRequested.Add([this](std::wstring const& key)
        {
            wan::Services().pendingOpenThread.clear();
            SelectThread(key);
        });
        m_modemToken = s.modem->StatusChanged.Add([this] { UpdateSendEnabled(); });

        RefreshThreads();
        if (!s.pendingOpenThread.empty())
        {
            auto key = s.pendingOpenThread;
            s.pendingOpenThread.clear();
            SelectThread(key);
        }
        else
        {
            RefreshConversation();
        }
    }

    void MessagesPage::Page_Unloaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        s.MessagesChanged.Remove(m_messagesToken);
        s.OpenThreadRequested.Remove(m_openToken);
        s.modem->StatusChanged.Remove(m_modemToken);
        s.threadInView.clear();
    }

    Style MessagesPage::LocalStyle(wchar_t const* key)
    {
        return Resources().Lookup(box_value(key)).as<Microsoft::UI::Xaml::Style>();
    }

    void MessagesPage::RefreshThreads()
    {
        auto& s = wan::Services();
        if (!s.db) return;
        std::vector<wan::core::Thread> threads;
        try
        {
            threads = s.db->Threads(wan::core::Trim(SearchBox().Text()));
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading conversations failed: " + wan::Widen(e.what()));
            return;
        }
        m_refreshing = true;
        ThreadList().Items().Clear();
        for (auto const& t : threads)
        {
            auto row = MakeThreadRow(t);
            ThreadList().Items().Append(row);
            if (t.threadKey == m_threadKey)
            {
                ThreadList().SelectedItem(row);
            }
        }
        m_refreshing = false;
    }

    ListViewItem MessagesPage::MakeThreadRow(wan::core::Thread const& t)
    {
        Grid top;
        top.ColumnSpacing(8);
        ColumnDefinition nameCol;
        nameCol.Width(GridLength{ 1, GridUnitType::Star });
        ColumnDefinition timeCol;
        timeCol.Width(GridLength{ 0, GridUnitType::Auto }); // a default ColumnDefinition is "*", not Auto
        top.ColumnDefinitions().Append(nameCol);
        top.ColumnDefinitions().Append(timeCol);
        TextBlock name;
        name.Style(LocalStyle(L"RowName"));
        name.Text(t.displayName);
        TextBlock time;
        time.Style(LocalStyle(L"RowPreview"));
        time.Text(wan::FormatTime(t.lastMessageAtMs));
        Grid::SetColumn(time, 1);
        top.Children().Append(name);
        top.Children().Append(time);

        Grid bottom;
        bottom.ColumnSpacing(8);
        ColumnDefinition previewCol;
        previewCol.Width(GridLength{ 1, GridUnitType::Star });
        ColumnDefinition badgeCol;
        badgeCol.Width(GridLength{ 0, GridUnitType::Auto });
        bottom.ColumnDefinitions().Append(previewCol);
        bottom.ColumnDefinitions().Append(badgeCol);
        TextBlock preview;
        preview.Style(LocalStyle(L"RowPreview"));
        preview.Text(wan::core::FirstLine(t.lastBody));
        bottom.Children().Append(preview);
        if (t.unreadCount > 0)
        {
            InfoBadge badge;
            badge.Value(t.unreadCount);
            Grid::SetColumn(badge, 1);
            bottom.Children().Append(badge);
        }

        StackPanel panel;
        panel.Padding(Thickness{ 0, 6, 0, 6 });
        panel.Spacing(2);
        panel.Children().Append(top);
        panel.Children().Append(bottom);

        ListViewItem item;
        item.Tag(box_value(hstring(t.threadKey)));
        item.Content(panel);
        return item;
    }

    void MessagesPage::SelectThread(std::wstring const& threadKey)
    {
        m_threadKey = threadKey;
        m_newMode = false;
        RefreshThreads();
        RefreshConversation();
    }

    void MessagesPage::ThreadList_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (m_refreshing) return;
        auto item = ThreadList().SelectedItem().try_as<ListViewItem>();
        if (!item) return;
        m_threadKey = unbox_value<hstring>(item.Tag());
        m_newMode = false;
        RefreshConversation();
    }

    void MessagesPage::SearchBox_TextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        RefreshThreads();
    }

    void MessagesPage::RefreshConversation()
    {
        auto& s = wan::Services();
        Bubbles().Children().Clear();
        m_replyAddress.clear();

        ToBox().Visibility(m_newMode ? Visibility::Visible : Visibility::Collapsed);
        ThreadTitle().Visibility(m_newMode ? Visibility::Collapsed : Visibility::Visible);
        s.threadInView = m_newMode ? std::wstring() : m_threadKey;

        if (m_newMode || m_threadKey.empty() || !s.db)
        {
            if (!m_newMode) ThreadTitle().Text(L"Select a conversation");
            UpdateSendEnabled();
            return;
        }

        std::vector<wan::core::Message> messages;
        try
        {
            messages = s.db->MessagesInThread(m_threadKey);
            if (s.windowFocused && s.db->MarkThreadRead(m_threadKey) > 0)
            {
                RefreshThreads(); // clear the unread badge
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading the conversation failed: " + wan::Widen(e.what()));
            return;
        }

        if (!messages.empty())
        {
            m_replyAddress = messages.back().address;
        }
        ThreadTitle().Text(m_replyAddress.empty() ? m_threadKey : m_replyAddress);
        for (auto const& m : messages)
        {
            Bubbles().Children().Append(MakeBubble(m));
        }
        BubbleScroller().UpdateLayout();
        BubbleScroller().ChangeView(nullptr, BubbleScroller().ScrollableHeight(), nullptr);
        UpdateSendEnabled();
    }

    UIElement MessagesPage::MakeBubble(wan::core::Message const& m)
    {
        const bool out = m.direction == Direction::Out;
        const bool failed = m.status == MessageStatus::Failed;
        const bool onAccent = out && !failed;

        StackPanel content;
        content.Spacing(2);

        TextBlock body;
        body.Style(LocalStyle(onAccent ? L"OnAccentText" : L"BubbleText"));
        body.Text(m.body);
        content.Children().Append(body);

        std::wstring meta = wan::FormatTime(m.timestampMs);
        if (m.status == MessageStatus::Sending) meta += L" · Sending…";
        else if (failed) meta += L" · Not sent";
        TextBlock metaText;
        metaText.Style(LocalStyle(onAccent ? L"OnAccentMetaText" : L"MetaText"));
        metaText.Text(meta);
        content.Children().Append(metaText);

        if (failed)
        {
            TextBlock error;
            error.Style(LocalStyle(L"ErrorText"));
            error.Text(m.errorDetail);
            content.Children().Append(error);
            HyperlinkButton retry;
            retry.Content(box_value(L"Retry"));
            retry.Padding(Thickness{ 0, 0, 0, 0 });
            const int64_t id = m.id;
            retry.Click([id](IInspectable const&, RoutedEventArgs const&) { wan::Services().sms->Retry(id); });
            content.Children().Append(retry);
        }

        Border bubble;
        bubble.Style(LocalStyle(failed ? L"FailedBubble" : (out ? L"OutBubble" : L"InBubble")));
        bubble.Child(content);
        return bubble;
    }

    void MessagesPage::UpdateSendEnabled()
    {
        auto& s = wan::Services();
        const bool ready = s.db && s.modem->Status().ready;
        const bool hasRecipient = m_newMode
            ? wan::core::IsPhoneNumberKey(wan::core::ThreadKeyFor(ToBox().Text()))
            : (!m_replyAddress.empty() && wan::core::IsPhoneNumberKey(m_threadKey)); // "dtac" cannot receive SMS
        const bool hasBody = !wan::core::Trim(ComposeBox().Text()).empty();
        SendButton().IsEnabled(ready && hasRecipient && hasBody);
    }

    void MessagesPage::NewButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_newMode = true;
        m_threadKey.clear();
        m_refreshing = true;
        ThreadList().SelectedItem(nullptr);
        m_refreshing = false;
        ToBox().Text(L"");
        RefreshConversation();
        ToBox().Focus(FocusState::Programmatic);
    }

    void MessagesPage::ComposeInput_TextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        UpdateSendEnabled();
    }

    void MessagesPage::SendButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        const auto body = wan::core::Trim(ComposeBox().Text());
        const auto address = m_newMode ? wan::core::Trim(ToBox().Text()) : m_replyAddress;
        if (body.empty() || address.empty()) return;

        ComposeBox().Text(L"");
        m_newMode = false;
        m_threadKey = wan::core::ThreadKeyFor(address);
        // Send() inserts the "sending" row before its first suspension, so the refresh below shows it.
        wan::Services().sms->Send(address, body);
        RefreshThreads();
        RefreshConversation();
    }
}
