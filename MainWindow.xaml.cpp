#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "Services/AppServices.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Windowing;

namespace winrt::WANUtility::implementation
{
    void MainWindow::InitializeComponent()
    {
        MainWindowT::InitializeComponent();

        this->try_as<::IWindowNative>()->get_WindowHandle(&m_hwnd);
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        AppWindow().Resize({ 1100, 720 });
        AppWindow().Closing({ this, &MainWindow::OnClosing });
        Activated([](IInspectable const&, WindowActivatedEventArgs const& e)
        {
            wan::Services().windowFocused = e.WindowActivationState() != WindowActivationState::Deactivated;
        });

        auto& s = wan::Services();
        s.ErrorRaised.Add([this](std::wstring const& message) { ShowError(message); });
        if (!s.fatalError.empty())
        {
            ShowError(s.fatalError);
        }

        // [window-init]

        Nav().SelectedItem(Nav().MenuItems().GetAt(0));
    }

    void MainWindow::Nav_SelectionChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& args)
    {
        if (args.IsSettingsSelected())
        {
            ContentFrame().Navigate(xaml_typename<WANUtility::SettingsPage>());
            return;
        }
        auto item = args.SelectedItemContainer();
        if (!item)
        {
            return;
        }
        auto tag = unbox_value<hstring>(item.Tag());
        if (tag == L"messages")
        {
            ContentFrame().Navigate(xaml_typename<WANUtility::MessagesPage>());
        }
        else if (tag == L"ussd")
        {
            ContentFrame().Navigate(xaml_typename<WANUtility::UssdPage>());
        }
    }

    void MainWindow::ShowAndFocus()
    {
        AppWindow().Show();
        Activate();
        SetForegroundWindow(m_hwnd);
    }

    void MainWindow::ShowThread(std::wstring const& threadKey)
    {
        auto& s = wan::Services();
        s.pendingOpenThread = threadKey;
        auto messagesItem = Nav().MenuItems().GetAt(0);
        if (Nav().SelectedItem() != messagesItem)
        {
            Nav().SelectedItem(messagesItem); // MessagesPage consumes pendingOpenThread when it loads
        }
        else
        {
            s.OpenThreadRequested.Raise(threadKey);
        }
    }

    void MainWindow::AllowCloseAndClose()
    {
        m_allowClose = true;
        Close();
    }

    void MainWindow::OnClosing(Microsoft::UI::Windowing::AppWindow const&, AppWindowClosingEventArgs const& args)
    {
        if (m_closeToTray && !m_allowClose)
        {
            args.Cancel(true);
            AppWindow().Hide();
            wan::Services().windowFocused = false;
        }
    }

    void MainWindow::ShowError(std::wstring const& message)
    {
        ErrorBar().Title(L"Something went wrong");
        ErrorBar().Message(message);
        ErrorBar().IsOpen(true);
    }
}
