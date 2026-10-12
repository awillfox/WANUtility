#pragma once
#include "Core/MessageDb.h"

#include <functional>
#include <string>

namespace wan
{
    class Notifier
    {
    public:
        // openThread runs on the UI thread with the clicked toast's thread key.
        void Initialize(std::function<void(std::wstring const&)> openThread);
        void ShowIncoming(core::Message const& message);
        void Shutdown();

        static std::wstring ThreadFromArguments(winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> const& arguments);

    private:
        bool m_registered = false;
    };
}
