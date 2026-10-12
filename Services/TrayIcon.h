#pragma once
#include <functional>
#include <string>

namespace wan
{
    // Notification-area icon. Left click -> onOpen; right click -> menu (Open / Exit).
    // Lives on the UI thread: its hidden window's messages are pumped by the XAML message loop.
    class TrayIcon
    {
    public:
        void Create(std::wstring const& iconPath, std::wstring const& tooltip, std::function<void()> onOpen, std::function<void()> onExit);
        void Remove();

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        void Add();
        void ShowMenu(int x, int y);

        HWND m_hwnd{};
        HICON m_icon{};
        UINT m_taskbarCreated{};
        std::wstring m_tooltip;
        std::function<void()> m_onOpen;
        std::function<void()> m_onExit;
    };
}
