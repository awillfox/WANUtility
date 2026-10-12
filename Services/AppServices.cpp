#include "pch.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Core/Log.h"

namespace wan
{
    AppServices& Services()
    {
        // Intentionally never destroyed: services hold COM objects, and static destructors run
        // after the apartment is torn down. SQLite has committed every statement already.
        static AppServices* instance = new AppServices();
        return *instance;
    }

    void ReportError(std::wstring const& message)
    {
        core::Log::Write(L"ERROR " + message);
        Services().ErrorRaised.Raise(message);
    }

    void InitializeServices()
    {
        auto& s = Services();
        s.dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        s.localState = std::filesystem::path(std::wstring(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()));
        core::Log::Init(s.localState / L"logs" / L"wanutility.log");
        core::Log::Write(L"--- WANUtility starting ---");

        try
        {
            s.db = std::make_unique<core::MessageDb>((s.localState / L"wanutility.db").wstring());
        }
        catch (core::DbError const& e)
        {
            s.fatalError = L"The message database could not be opened: " + Widen(e.what());
            core::Log::Write(L"FATAL " + s.fatalError);
        }

        s.modem = std::make_unique<ModemService>();
        s.modem->Initialize();

        // [services]
    }
}
