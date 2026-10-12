#include "pch.h"
#include "Services/StartupSetting.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;

namespace wan
{
    namespace
    {
        constexpr wchar_t kTaskId[] = L"WANUtilityStartup";
    }

    Windows::Foundation::IAsyncOperation<StartupTaskState> StartupSetting::GetStateAsync()
    {
        auto task = co_await StartupTask::GetAsync(kTaskId);
        co_return task.State();
    }

    Windows::Foundation::IAsyncOperation<StartupTaskState> StartupSetting::SetEnabledAsync(bool enabled)
    {
        auto task = co_await StartupTask::GetAsync(kTaskId);
        if (enabled)
        {
            co_return co_await task.RequestEnableAsync();
        }
        task.Disable();
        co_return task.State();
    }
}
