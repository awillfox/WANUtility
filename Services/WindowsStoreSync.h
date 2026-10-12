#pragma once
#include "Services/Signal.h"

#include <string>

namespace wan
{
    // Copies sent SMS into the Windows chat store. Transient failures stay "pending" and are retried
    // at start and every 5 minutes; an access-denied failure disables sync permanently (never retried).
    class WindowsStoreSync
    {
    public:
        winrt::fire_and_forget Start();
        winrt::fire_and_forget SyncPending();

        bool Enabled() const;
        std::wstring DisabledReason() const;

        Signal<> StateChanged; // raised after sync state or pending count may have changed

    private:
        winrt::Windows::Foundation::IAsyncAction SyncAsync();

        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        bool m_running = false;
    };
}
