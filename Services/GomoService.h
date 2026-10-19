#pragma once
#include "Core/GomoAccount.h"
#include "Services/Signal.h"

#include <cstdint>
#include <string>

namespace wan
{
    enum class GomoState { Idle, Loading, Loaded, Failed };

    // One GOMO refresh at a time. Lives in AppServices, so leaving the page does not cancel a refresh.
    // A refresh clears the previous values first: only a complete, validated result is ever shown.
    class GomoService
    {
    public:
        // GOMO identifies the subscriber from the AIS cellular connection, so the feature is off otherwise.
        bool Available() const;
        winrt::fire_and_forget Refresh(); // no-op while loading or not Available()

        GomoState State() const { return m_state; }
        core::GomoAccount const& Account() const { return m_account; } // empty unless Loaded
        std::wstring const& Error() const { return m_error; }           // set when Failed
        int64_t UpdatedMs() const { return m_updatedMs; }               // last success, 0 if none

        Signal<> Changed;

    private:
        GomoState m_state = GomoState::Idle;
        core::GomoAccount m_account;
        std::wstring m_error;
        int64_t m_updatedMs = 0;
    };
}
