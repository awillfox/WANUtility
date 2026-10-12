#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "Core/Log.h"
#include "Core/TextUtil.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/StartupSetting.h"
#include "Services/WindowsStoreSync.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using winrt::Windows::ApplicationModel::StartupTaskState;

namespace winrt::WANUtility::implementation
{
    void SettingsPage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        m_syncToken = s.storeSync->StateChanged.Add([this] { RenderSync(); });
        m_modemToken = s.modem->StatusChanged.Add([this] { RenderModem(); });
        LoadStartup();
        RenderSync();
        RenderCodes();
        RenderModem();
    }

    void SettingsPage::Page_Unloaded(IInspectable const&, RoutedEventArgs const&)
    {
        auto& s = wan::Services();
        s.storeSync->StateChanged.Remove(m_syncToken);
        s.modem->StatusChanged.Remove(m_modemToken);
    }

    fire_and_forget SettingsPage::LoadStartup()
    {
        auto strong = get_strong();
        try
        {
            ApplyStartupState(co_await wan::StartupSetting::GetStateAsync());
        }
        catch (hresult_error const& e)
        {
            StartupToggle().IsEnabled(false);
            StartupNote().Text(L"Unavailable: " + wan::DescribeError(e));
        }
    }

    fire_and_forget SettingsPage::StartupToggle_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_suppressToggle) co_return;
        auto strong = get_strong();
        try
        {
            ApplyStartupState(co_await wan::StartupSetting::SetEnabledAsync(StartupToggle().IsOn()));
        }
        catch (hresult_error const& e)
        {
            wan::ReportError(L"Changing start at sign-in failed: " + wan::DescribeError(e));
            LoadStartup();
        }
    }

    void SettingsPage::ApplyStartupState(StartupTaskState state)
    {
        wan::core::Log::Write(L"Startup task state " + std::to_wstring(static_cast<int>(state)));
        m_suppressToggle = true; // setting IsOn raises Toggled synchronously
        const bool on = state == StartupTaskState::Enabled || state == StartupTaskState::EnabledByPolicy;
        const bool locked = state == StartupTaskState::DisabledByUser || state == StartupTaskState::DisabledByPolicy ||
                            state == StartupTaskState::EnabledByPolicy;
        StartupToggle().IsOn(on);
        StartupToggle().IsEnabled(!locked);
        switch (state)
        {
        case StartupTaskState::DisabledByUser:
            StartupNote().Text(L"Turned off in Windows Settings › Apps › Startup. Turn it on there first.");
            break;
        case StartupTaskState::DisabledByPolicy:
            StartupNote().Text(L"Turned off by your organization's policy.");
            break;
        case StartupTaskState::EnabledByPolicy:
            StartupNote().Text(L"Turned on by your organization's policy.");
            break;
        default:
            StartupNote().Text(L"Starts in the system tray when you sign in to Windows.");
            break;
        }
        m_suppressToggle = false;
    }

    void SettingsPage::RenderSync()
    {
        auto& s = wan::Services();
        if (!s.db)
        {
            SyncStatus().Text(L"Unavailable: the message database could not be opened.");
            return;
        }
        try
        {
            const auto pending = s.db->PendingSync().size();
            if (s.storeSync->Enabled())
            {
                SyncStatus().Text(pending == 0
                    ? std::wstring(L"On. Sent messages are also copied to the Windows message store.")
                    : L"On. " + std::to_wstring(pending) + L" sent message(s) waiting to be copied; retrying every 5 minutes.");
            }
            else
            {
                SyncStatus().Text(L"Off. " + s.storeSync->DisabledReason() + L" Sent messages are still kept in WANUtility.");
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Reading sync status failed: " + wan::Widen(e.what()));
        }
    }

    void SettingsPage::RenderCodes()
    {
        CodesList().Children().Clear();
        auto& s = wan::Services();
        if (!s.db) return;
        try
        {
            auto codes = s.db->UssdCodes();
            if (codes.empty())
            {
                TextBlock none;
                none.Text(L"No saved codes. Save one from the USSD page.");
                CodesList().Children().Append(none);
            }
            for (auto const& c : codes)
            {
                Grid row;
                row.ColumnSpacing(8);
                for (auto width : { GridLength{ 110, GridUnitType::Pixel }, GridLength{ 1, GridUnitType::Star },
                                    GridLength{ 0, GridUnitType::Auto }, GridLength{ 0, GridUnitType::Auto } })
                {
                    ColumnDefinition column;
                    column.Width(width);
                    row.ColumnDefinitions().Append(column);
                }

                TextBlock code;
                code.Text(c.code);
                code.VerticalAlignment(VerticalAlignment::Center);
                TextBox label;
                label.Text(c.label);
                Grid::SetColumn(label, 1);
                Button save;
                save.Content(box_value(L"Save"));
                Grid::SetColumn(save, 2);
                Button remove;
                remove.Content(box_value(L"Remove"));
                Grid::SetColumn(remove, 3);

                const int64_t id = c.id;
                save.Click([this, id, label](IInspectable const&, RoutedEventArgs const&)
                {
                    const auto text = wan::core::Trim(label.Text());
                    if (text.empty()) return;
                    try { wan::Services().db->RenameUssdCode(id, text); }
                    catch (wan::core::DbError const& e) { wan::ReportError(L"Renaming the code failed: " + wan::Widen(e.what())); }
                    RenderCodes();
                });
                remove.Click([this, id](IInspectable const&, RoutedEventArgs const&)
                {
                    try { wan::Services().db->RemoveUssdCode(id); }
                    catch (wan::core::DbError const& e) { wan::ReportError(L"Removing the code failed: " + wan::Widen(e.what())); }
                    RenderCodes();
                });

                row.Children().Append(code);
                row.Children().Append(label);
                row.Children().Append(save);
                row.Children().Append(remove);
                CodesList().Children().Append(row);
            }
        }
        catch (wan::core::DbError const& e)
        {
            wan::ReportError(L"Loading saved codes failed: " + wan::Widen(e.what()));
        }
    }

    void SettingsPage::RenderModem()
    {
        const auto info = wan::Services().modem->Info();
        auto orUnknown = [](std::wstring const& v) { return v.empty() ? std::wstring(L"Unknown") : v; };
        ModemModel().Text(L"Model: " + orUnknown(info.model));
        ModemFirmware().Text(L"Firmware: " + orUnknown(info.firmware));
        ModemCarrier().Text(L"Carrier: " + orUnknown(info.carrier));
        ModemSmsc().Text(L"SMS center: " + orUnknown(info.smsc));
    }
}
