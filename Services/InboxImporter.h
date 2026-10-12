#pragma once
#include "Core/MessageDb.h"
#include "Services/Signal.h"

namespace wan
{
    // Copies incoming SMS from the Windows chat store into MessageDb.
    // Triggers: store MessageChanged (no details in this SDK), a 60 s timer, and Start().
    class InboxImporter
    {
    public:
        winrt::fire_and_forget Start();
        winrt::fire_and_forget Import();

        // Raised for each newly imported message, except during the very first import.
        Signal<core::Message> NewIncoming;

    private:
        winrt::Windows::Foundation::IAsyncAction ImportAsync();

        winrt::Windows::ApplicationModel::Chat::ChatMessageStore m_store{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        bool m_running = false;
        bool m_again = false;
    };
}
