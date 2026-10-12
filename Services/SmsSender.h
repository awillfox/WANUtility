#pragma once
#include <string>

namespace wan
{
    // Send flow: save as "sending" -> transmit -> "sent"/"failed" -> (Task 12) copy to Windows store.
    class SmsSender
    {
    public:
        winrt::fire_and_forget Send(std::wstring address, std::wstring body);
        winrt::fire_and_forget Retry(int64_t messageId);

    private:
        winrt::Windows::Foundation::IAsyncAction TransmitAsync(int64_t messageId);
    };

    std::wstring DescribeSendFailure(winrt::Windows::Devices::Sms::SmsSendMessageResult const& result);
}
