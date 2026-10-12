#include "pch.h"
#include "Services/SmsSender.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/ModemService.h"
#include "Services/TimeUtil.h"
#include "Services/WindowsStoreSync.h"
#include "Core/Log.h"
#include "Core/PhoneNumber.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Sms;

namespace wan
{
    std::wstring DescribeSendFailure(SmsSendMessageResult const& r)
    {
        return L"Modem error " + std::to_wstring(static_cast<int>(r.ModemErrorCode())) +
               L", network cause " + std::to_wstring(r.NetworkCauseCode()) +
               L", transport cause " + std::to_wstring(r.TransportFailureCause()) +
               (r.IsErrorTransient() ? L" (temporary — try again)" : L"");
    }

    winrt::fire_and_forget SmsSender::Send(std::wstring address, std::wstring body)
    {
        auto& s = Services();
        if (!s.db) co_return;
        int64_t id = 0;
        try
        {
            id = s.db->InsertOutgoing(address, body, NowMs());
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Saving the message failed: " + Widen(e.what()));
            co_return;
        }
        s.MessagesChanged.Raise(core::ThreadKeyFor(address));
        co_await TransmitAsync(id);
    }

    winrt::fire_and_forget SmsSender::Retry(int64_t messageId)
    {
        auto& s = Services();
        if (!s.db) co_return;
        std::wstring threadKey;
        try
        {
            s.db->MarkSending(messageId);
            if (auto m = s.db->MessageById(messageId)) threadKey = m->threadKey;
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Retrying failed: " + Widen(e.what()));
            co_return;
        }
        s.MessagesChanged.Raise(threadKey);
        co_await TransmitAsync(messageId);
    }

    Windows::Foundation::IAsyncAction SmsSender::TransmitAsync(int64_t messageId)
    {
        auto& s = Services();
        std::optional<core::Message> message;
        try
        {
            message = s.db->MessageById(messageId);
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Reading the message failed: " + Widen(e.what()));
            co_return;
        }
        if (!message) co_return;

        std::wstring failure;
        auto device = s.modem->SmsDevice();
        if (!device)
        {
            failure = L"No cellular modem was found.";
        }
        else if (!s.modem->Status().ready)
        {
            failure = s.modem->Status().problem;
        }
        else
        {
            try
            {
                SmsTextMessage2 text;
                text.To(message->address);
                text.Body(message->body);
                auto result = co_await device.SendMessageAndGetResultAsync(text);
                if (!result.IsSuccessful())
                {
                    failure = DescribeSendFailure(result);
                }
            }
            catch (hresult_error const& e)
            {
                failure = DescribeError(e);
            }
        }

        try
        {
            if (failure.empty()) s.db->MarkSent(messageId);
            else s.db->MarkFailed(messageId, failure);
        }
        catch (core::DbError const& e)
        {
            ReportError(L"Updating the message failed: " + Widen(e.what()));
        }
        core::Log::Write(L"SMS " + std::to_wstring(messageId) + L" to " + message->address + L" (" +
                         std::to_wstring(message->body.size()) + L" chars): " + (failure.empty() ? std::wstring(L"sent") : L"FAILED " + failure));
        s.MessagesChanged.Raise(message->threadKey);

        if (failure.empty())
        {
            s.storeSync->SyncPending();
        }
    }
}
