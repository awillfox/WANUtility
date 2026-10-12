#include "pch.h"
#include "App.xaml.h"

#include <wil/resource.h>

namespace
{
    // Returns true when another WANUtility process already runs; this launch's activation
    // (normal launch, toast click, startup task) is forwarded to it and this process exits.
    bool RedirectToRunningInstance()
    {
        using namespace winrt::Microsoft::Windows::AppLifecycle;
        auto args = AppInstance::GetCurrent().GetActivatedEventArgs();
        auto main = AppInstance::FindOrRegisterForKey(L"WANUtility.Main");
        if (main.IsCurrent())
        {
            return false;
        }

        // We were launched by the user, so we may hand foreground rights to the running instance.
        AllowSetForegroundWindow(main.ProcessId());

        // RedirectActivationToAsync must not block this STA thread: run it on a worker thread and
        // wait while pumping COM (Windows App SDK single-instancing pattern).
        wil::unique_event done;
        done.create();
        HANDLE doneHandle = done.get();
        std::thread worker([args, main, doneHandle]
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            main.RedirectActivationToAsync(args).get();
            SetEvent(doneHandle);
            winrt::uninit_apartment();
        });
        DWORD index = 0;
        CoWaitForMultipleObjects(CWMO_DEFAULT, INFINITE, 1, &doneHandle, &index);
        worker.join();
        return true;
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (RedirectToRunningInstance())
    {
        return 0;
    }
    // Start XAML ourselves rather than via the generated wXamlGeneratedMain(): with
    // DISABLE_XAML_GENERATED_MAIN it only constructs App when its has_parameterless_ctor_v
    // trait is true, and that trait evaluates false for this C++/WinRT App (observed: App's
    // constructor never ran and Microsoft.UI.Xaml.dll failed with 0xC000027B).
    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
    {
        winrt::make<winrt::WANUtility::implementation::App>();
    });
    return 0;
}
