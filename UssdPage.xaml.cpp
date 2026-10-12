#include "pch.h"
#include "UssdPage.xaml.h"
#if __has_include("UssdPage.g.cpp")
#include "UssdPage.g.cpp"
#endif

#include "Core/TextUtil.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/TimeUtil.h"
#include "Services/UssdSessionManager.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::WANUtility::implementation
{
    void UssdPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        m_ussdToken = s.ussd->Changed.Add([this]
        {
            Render();
            if (wan::Services().ussd->State() == wan::UssdState::Ended) RenderRecent();
        });
        m_modemToken = s.modem->StatusChanged.Add([this] { Render(); });
        RenderChips();
        RenderRecent();
        Render();
    }

    void UssdPage::Page_Unloaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        s.ussd->Changed.Remove(m_ussdToken);
        s.modem->StatusChanged.Remove(m_modemToken);
    }

    void UssdPage::Render()
    {
        auto& s = wan::Services();
        auto& u = *s.ussd;
        const bool ready = s.modem->Status().ready;
        const bool busy = u.State() == wan::UssdState::Sending;
        const bool awaiting = u.State() == wan::UssdState::AwaitingReply;

        StatusLine().Text(u.StatusLine());
        Busy().IsActive(busy);
        ReplyText().Text(u.State() == wan::UssdState::Idle ? std::wstring(L"Dial a code or tap a saved one.") : u.ReplyText());

        Options().Children().Clear();
        if (awaiting)
        {
            for (auto const& option : u.Options())
            {
                Button b;
                b.Content(box_value(hstring(option.key + L" · " + option.label)));
                b.HorizontalAlignment(HorizontalAlignment::Stretch);
                b.HorizontalContentAlignment(HorizontalAlignment::Left);
                b.IsEnabled(ready && !busy);
                auto key = option.key;
                b.Click([key](IInspectable const&, RoutedEventArgs const&) { wan::Services().ussd->Reply(key); });
                Options().Children().Append(b);
            }
        }
        ReplyRow().Visibility(awaiting ? Visibility::Visible : Visibility::Collapsed);
        SendReplyButton().IsEnabled(ready && !busy);

        const bool canDial = ready && !busy;
        DialButton().IsEnabled(canDial && !wan::core::Trim(DialBox().Text()).empty());
        for (auto const& child : Chips().Children())
        {
            child.as<Control>().IsEnabled(canDial);
        }
    }

    void UssdPage::RenderChips()
    {
        Chips().Children().Clear();
        auto& s = wan::Services();
        if (!s.db) return;
        try
        {
            for (auto const& c : s.db->UssdCodes())
            {
                Button chip;
                chip.Content(box_value(hstring(c.label + L"  " + c.code)));
                chip.CornerRadius(Microsoft::UI::Xaml::CornerRadius{ 14, 14, 14, 14 });
                auto code = c.code;
                chip.Click([this, code](IInspectable const&, RoutedEventArgs const&)
                {
                    DialBox().Text(code);
                    wan::Services().ussd->Dial(code);
                });
                Chips().Children().Append(chip);
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading saved codes failed: " + wan::Widen(e.what()));
        }
    }

    void UssdPage::RenderRecent()
    {
        Recent().Children().Clear();
        auto& s = wan::Services();
        if (!s.db) return;
        try
        {
            for (auto const& h : s.db->RecentUssd(5))
            {
                TextBlock t;
                t.Text(h.code + L" → " + wan::core::FirstLine(h.finalReply) + L" · " + wan::FormatTime(h.timestampMs));
                t.TextTrimming(TextTrimming::CharacterEllipsis);
                t.Opacity(0.8);
                Recent().Children().Append(t);
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading USSD history failed: " + wan::Widen(e.what()));
        }
    }

    void UssdPage::DialBox_TextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        Render();
    }

    void UssdPage::DialButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto code = wan::core::Trim(DialBox().Text());
        if (code.empty()) return;
        wan::Services().ussd->Dial(code);
    }

    void UssdPage::SendReplyButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto text = wan::core::Trim(ReplyBox().Text());
        if (text.empty()) return;
        ReplyBox().Text(L"");
        wan::Services().ussd->Reply(text);
    }

    void UssdPage::EndButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        wan::Services().ussd->End();
    }

    fire_and_forget UssdPage::SaveCodeButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto strong = get_strong();
        auto code = wan::core::Trim(DialBox().Text());
        if (code.empty() || !wan::Services().db) co_return;

        TextBox label;
        label.PlaceholderText(L"Label, e.g. Balance");
        ContentDialog dialog;
        dialog.XamlRoot(XamlRoot());
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
        dialog.Title(box_value(hstring(L"Save " + code)));
        dialog.Content(label);
        dialog.PrimaryButtonText(L"Save");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);
        if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) co_return;

        auto text = wan::core::Trim(label.Text());
        try
        {
            wan::Services().db->AddUssdCode(code, text.empty() ? code : text);
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Saving the code failed: " + wan::Widen(e.what()));
        }
        RenderChips();
        Render();
    }
}
