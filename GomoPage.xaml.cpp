#include "pch.h"
#include "GomoPage.xaml.h"
#if __has_include("GomoPage.g.cpp")
#include "GomoPage.g.cpp"
#endif

#include "Services/AppServices.h"
#include "Services/GomoService.h"
#include "Services/TimeUtil.h"


using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace
{
    TextBlock Line(std::wstring const& text, bool secondary = false)
    {
        TextBlock t;
        t.Text(text);
        t.TextWrapping(TextWrapping::Wrap);
        t.IsTextSelectionEnabled(true);
        if (secondary)
        {
            t.Style(Application::Current().Resources().Lookup(box_value(L"CaptionTextBlockStyle")).as<Style>());
        }
        return t;
    }

    void AddJoined(StackPanel const& panel, std::initializer_list<std::wstring> parts, bool secondary)
    {
        std::wstring text;
        for (auto const& part : parts)
        {
            if (part.empty()) continue;
            if (!text.empty()) text += L" · ";
            text += part;
        }
        if (!text.empty()) panel.Children().Append(Line(text, secondary));
    }

    Border PackageCard(wan::core::GomoPackage const& p)
    {
        StackPanel body;
        body.Spacing(4);
        TextBlock name = Line(p.name);
        name.Style(Application::Current().Resources().Lookup(box_value(L"BodyStrongTextBlockStyle")).as<Style>());
        body.Children().Append(name);
        if (!p.description.empty()) body.Children().Append(Line(p.description, true));
        AddJoined(body, { p.status,
                          p.startDate.empty() ? L"" : L"From " + wan::core::FormatGomoDate(p.startDate),
                          p.endDate.empty() ? L"" : L"Until " + wan::core::FormatGomoDate(p.endDate) }, true);
        AddJoined(body, { p.allowance.empty() ? L"" : L"Allowance " + p.allowance,
                          p.speed.empty() ? L"" : L"Speed " + p.speed }, true);
        for (auto const& line : p.remaining) body.Children().Append(Line(line));

        Border card;
        card.Background(Application::Current().Resources().Lookup(box_value(L"CardBackgroundFillColorDefaultBrush")).as<Media::Brush>());
        card.BorderBrush(Application::Current().Resources().Lookup(box_value(L"CardStrokeColorDefaultBrush")).as<Media::Brush>());
        card.BorderThickness(ThicknessHelper::FromUniformLength(1));
        card.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
        card.Padding(ThicknessHelper::FromUniformLength(16));
        card.Child(body);
        return card;
    }
}

namespace winrt::WANUtility::implementation
{
    void GomoPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        m_gomoToken = s.gomo->Changed.Add([this] { Render(); });
        m_modemToken = s.modem->StatusChanged.Add([this] { RefreshIfFirstVisit(); Render(); });
        RefreshIfFirstVisit();
        Render();
    }

    void GomoPage::Page_Unloaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        s.gomo->Changed.Remove(m_gomoToken);
        s.modem->StatusChanged.Remove(m_modemToken);
    }

    void GomoPage::RefreshIfFirstVisit()
    {
        auto& gomo = *wan::Services().gomo;
        if (gomo.State() == wan::GomoState::Idle && gomo.Available())
        {
            gomo.Refresh();
        }
    }

    void GomoPage::RefreshButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        wan::Services().gomo->Refresh();
    }

    void GomoPage::Render()
    {
        auto const& gomo = *wan::Services().gomo;
        const bool available = gomo.Available();
        // A refresh that started before the connection changed still finishes, but is not shown.
        const auto state = available ? gomo.State() : wan::GomoState::Idle;
        const bool loading = state == wan::GomoState::Loading;

        UnavailableBar().IsOpen(!available);
        Busy().IsActive(loading);
        RefreshButton().IsEnabled(available && !loading);
        std::wstring status = loading ? L"Loading your account over the cellular connection…" : L"";
        if (available && !loading && gomo.UpdatedMs() > 0) status = L"Last updated " + wan::FormatTime(gomo.UpdatedMs());
        StatusLine().Text(status);
        StatusLine().Visibility(status.empty() ? Visibility::Collapsed : Visibility::Visible);

        FailureBar().Message(gomo.Error());
        FailureBar().IsOpen(state == wan::GomoState::Failed);

        const bool loaded = state == wan::GomoState::Loaded;
        Details().Visibility(loaded ? Visibility::Visible : Visibility::Collapsed);
        Packages().Children().Clear();
        if (!loaded)
        {
            return;
        }
        auto const& a = gomo.Account();
        DataRemaining().Text(a.data.remaining);
        DataMaximum().Text(a.data.maximum.empty() ? L"" : L"of " + a.data.maximum);
        CallsRemaining().Text(a.calls.remaining);
        CallsMaximum().Text(a.calls.maximum.empty() ? L"" : L"of " + a.calls.maximum);
        if (a.packages.empty())
        {
            Packages().Children().Append(Line(L"No packages.", true));
        }
        for (auto const& p : a.packages)
        {
            Packages().Children().Append(PackageCard(p));
        }
    }
}
