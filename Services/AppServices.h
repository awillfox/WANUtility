#pragma once
#include "Core/MessageDb.h"
#include "Services/Signal.h"
#include "Services/ModemService.h"
#include "Services/UssdSessionManager.h"
#include "Services/InboxImporter.h"

#include <filesystem>
#include <memory>
#include <string>

namespace wan
{
    // Process-wide service container. Created once in App::OnLaunched, used on the UI thread only.
    struct AppServices
    {
        winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
        std::filesystem::path localState;
        std::unique_ptr<core::MessageDb> db;   // null when fatalError is set
        std::wstring fatalError;               // startup failure shown in the error bar

        bool windowFocused = false;            // main window visible and active
        std::wstring threadInView;             // thread shown on MessagesPage, empty otherwise
        std::wstring pendingOpenThread;        // thread to open when MessagesPage loads

        Signal<std::wstring> ErrorRaised;        // non-fatal error text for the error bar
        Signal<std::wstring> MessagesChanged;    // thread key, or L"" when several threads changed
        Signal<std::wstring> OpenThreadRequested;

        std::unique_ptr<ModemService> modem;

        std::unique_ptr<UssdSessionManager> ussd;

        std::unique_ptr<InboxImporter> importer;

        // [members]
    };

    AppServices& Services();
    void InitializeServices();
    void ReportError(std::wstring const& message);
}
