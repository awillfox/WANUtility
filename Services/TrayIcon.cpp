#include "pch.h"
#include "Services/TrayIcon.h"
#include "Core/Log.h"

namespace wan
{
    namespace
    {
        constexpr UINT WM_TRAY = WM_APP + 1;
        constexpr UINT kOpen = 1001;
        constexpr UINT kExit = 1002;
        constexpr wchar_t kClassName[] = L"WANUtilityTrayWindow";
    }

    void TrayIcon::Create(std::wstring const& iconPath, std::wstring const& tooltip, std::function<void()> onOpen, std::function<void()> onExit)
    {
        m_tooltip = tooltip;
        m_onOpen = std::move(onOpen);
        m_onExit = std::move(onExit);

        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = &TrayIcon::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc);

        // A hidden top-level window rather than HWND_MESSAGE: TrackPopupMenu needs a window that can
        // become foreground, otherwise the menu does not close when the user clicks elsewhere.
        m_hwnd = CreateWindowExW(0, kClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, this);
        m_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

        m_icon = static_cast<HICON>(LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));
        if (!m_icon)
        {
            core::Log::Write(L"Tray icon could not be loaded from " + iconPath + L"; using the default icon");
            m_icon = LoadIconW(nullptr, IDI_APPLICATION);
        }
        Add();
    }

    void TrayIcon::Add()
    {
        NOTIFYICONDATAW nid{ sizeof(nid) };
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        nid.uCallbackMessage = WM_TRAY;
        nid.hIcon = m_icon;
        wcsncpy_s(nid.szTip, m_tooltip.c_str(), _TRUNCATE);
        if (!Shell_NotifyIconW(NIM_ADD, &nid))
        {
            core::Log::Write(L"Shell_NotifyIcon(NIM_ADD) failed");
        }
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }

    void TrayIcon::Remove()
    {
        if (!m_hwnd)
        {
            return;
        }
        NOTIFYICONDATAW nid{ sizeof(nid) };
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    void TrayIcon::ShowMenu(int x, int y)
    {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, kOpen, L"Open WANUtility");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kExit, L"Exit");
        SetForegroundWindow(m_hwnd);
        const UINT command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, x, y, 0, m_hwnd, nullptr));
        PostMessageW(m_hwnd, WM_NULL, 0, 0);
        DestroyMenu(menu);
        if (command == kOpen) m_onOpen();
        else if (command == kExit) m_onExit();
    }

    LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
        {
            if (message == WM_TRAY)
            {
                switch (LOWORD(lParam)) // NOTIFYICON_VERSION_4: event in LOWORD(lParam), anchor in wParam
                {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    self->m_onOpen();
                    return 0;
                case WM_CONTEXTMENU:
                    self->ShowMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
                    return 0;
                }
            }
            else if (message == self->m_taskbarCreated)
            {
                self->Add(); // Explorer restarted: the icon must be added again
                return 0;
            }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}
