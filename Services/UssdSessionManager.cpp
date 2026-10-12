#include "pch.h"
#include "Services/UssdSessionManager.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/TimeUtil.h"
#include "Core/Log.h"
#include "Core/Ucs2.h"

using namespace winrt;
using namespace winrt::Windows::Networking::NetworkOperators;

namespace wan
{
    std::wstring DecodeUssdPayload(UssdMessage const& message)
    {
        if (!message)
        {
            return {};
        }
        auto raw = message.GetPayload();
        const auto dcs = message.DataCodingScheme();
        if (core::IsUcs2Dcs(dcs))
        {
            return core::DecodeUtf16BE(std::span<const uint8_t>(raw.data(), raw.size()));
        }
        std::wstring text{ message.PayloadAsText() };
        if (!text.empty() || raw.size() == 0)
        {
            return text;
        }
        wchar_t buf[48];
        swprintf_s(buf, L"Unreadable reply (DCS 0x%02X)", dcs);
        return buf;
    }

    winrt::fire_and_forget UssdSessionManager::Dial(std::wstring code)
    {
        if (m_state == UssdState::Sending)
        {
            co_return;
        }
        if (m_session)
        {
            m_session.Close();
            m_session = nullptr;
        }
        m_path.clear();
        m_options.clear();
        m_text.clear();

        const auto accountId = Services().modem->NetworkAccountId();
        if (accountId.empty())
        {
            m_path.push_back(code);
            Finish(L"No cellular network account was found.");
            co_return;
        }
        try
        {
            m_session = UssdSession::CreateFromNetworkAccountId(accountId);
        }
        catch (hresult_error const& e)
        {
            m_path.push_back(code);
            Finish(L"Could not start USSD: " + DescribeError(e));
            co_return;
        }
        co_await SendAsync(code);
    }

    winrt::fire_and_forget UssdSessionManager::Reply(std::wstring text)
    {
        if (m_state != UssdState::AwaitingReply || !m_session)
        {
            co_return;
        }
        co_await SendAsync(text);
    }

    void UssdSessionManager::End()
    {
        if (m_state == UssdState::AwaitingReply)
        {
            Finish(m_text.empty() ? std::wstring(L"Session ended.") : m_text);
        }
    }

    Windows::Foundation::IAsyncAction UssdSessionManager::SendAsync(std::wstring text)
    {
        m_path.push_back(text);
        m_options.clear();
        SetState(UssdState::Sending);
        core::Log::Write(L"USSD send '" + text + L"'");
        try
        {
            auto reply = co_await m_session.SendMessageAndGetReplyAsync(UssdMessage{ text });
            HandleReply(reply);
        }
        catch (hresult_error const& e)
        {
            Finish(L"USSD failed: " + DescribeError(e));
        }
    }

    void UssdSessionManager::HandleReply(UssdReply const& reply)
    {
        const auto text = DecodeUssdPayload(reply.Message());
        const auto code = reply.ResultCode();
        core::Log::Write(L"USSD reply code=" + std::to_wstring(static_cast<int>(code)) + L" text='" + text + L"'");
        switch (code)
        {
        case UssdResultCode::ActionRequired:
            m_text = text;
            m_options = core::ParseUssdMenu(text);
            SetState(UssdState::AwaitingReply);
            return;
        case UssdResultCode::NoActionRequired:
            Finish(text.empty() ? std::wstring(L"(The network sent no text.)") : text);
            return;
        case UssdResultCode::Terminated:
            Finish(text.empty() ? std::wstring(L"Session ended by network.") : text + L"\n\nSession ended by network.");
            return;
        case UssdResultCode::OtherLocalClient:
            Finish(L"Another app is using USSD. Close it and try again.");
            return;
        case UssdResultCode::OperationNotSupported:
            Finish(L"Not supported by the network or modem.");
            return;
        case UssdResultCode::NetworkTimeout:
            Finish(L"Network timed out. Dial again.");
            return;
        }
        Finish(text);
    }

    void UssdSessionManager::Finish(std::wstring const& text)
    {
        m_text = text;
        m_options.clear();
        if (m_session)
        {
            m_session.Close();
            m_session = nullptr;
        }
        if (!m_path.empty() && Services().db)
        {
            try
            {
                Services().db->AddUssdHistory(m_path.front(), text, NowMs());
            }
            catch (core::DbError const& e)
            {
                ReportError(L"Saving USSD history failed: " + Widen(e.what()));
            }
        }
        SetState(UssdState::Ended);
    }

    void UssdSessionManager::SetState(UssdState state)
    {
        m_state = state;
        Changed.Raise();
    }

    std::wstring UssdSessionManager::StatusLine() const
    {
        std::wstring path;
        for (size_t i = 0; i < m_path.size(); ++i)
        {
            if (i > 0) path += L" → ";
            path += m_path[i];
        }
        switch (m_state)
        {
        case UssdState::Idle: return L"No active session";
        case UssdState::Sending: return L"Sending " + (m_path.empty() ? std::wstring() : m_path.back()) + L"…";
        case UssdState::AwaitingReply: return L"Session active · step " + std::to_wstring(m_path.size()) + L" · " + path;
        case UssdState::Ended: return L"Ended · " + path;
        }
        return {};
    }
}
