#pragma once
#include "Core/UssdMenuParser.h"
#include "Services/Signal.h"

#include <string>
#include <vector>

namespace wan
{
    enum class UssdState { Idle, Sending, AwaitingReply, Ended };

    // One USSD session at a time. Lives in AppServices, so leaving the page keeps the session.
    class UssdSessionManager
    {
    public:
        winrt::fire_and_forget Dial(std::wstring code);
        winrt::fire_and_forget Reply(std::wstring text);
        void End();

        UssdState State() const { return m_state; }
        std::wstring const& ReplyText() const { return m_text; }
        std::vector<core::UssdOption> const& Options() const { return m_options; }
        std::wstring StatusLine() const;

        Signal<> Changed;

    private:
        winrt::Windows::Foundation::IAsyncAction SendAsync(std::wstring text);
        void HandleReply(winrt::Windows::Networking::NetworkOperators::UssdReply const& reply);
        void Finish(std::wstring const& text);
        void SetState(UssdState state);

        winrt::Windows::Networking::NetworkOperators::UssdSession m_session{ nullptr };
        UssdState m_state = UssdState::Idle;
        std::wstring m_text;
        std::vector<core::UssdOption> m_options;
        std::vector<std::wstring> m_path; // codes/replies sent in this session
    };

    // UCS-2 replies are decoded from raw bytes because PayloadAsText() returns an empty string
    // for them (observed with dtac, DCS 0x48).
    std::wstring DecodeUssdPayload(winrt::Windows::Networking::NetworkOperators::UssdMessage const& message);
}
