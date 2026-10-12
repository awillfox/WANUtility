#pragma once

namespace wan
{
    // Wraps the packaged StartupTask "WANUtilityStartup" declared in Package.appxmanifest.
    class StartupSetting
    {
    public:
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::ApplicationModel::StartupTaskState> GetStateAsync();
        // Enabling may be refused (DisabledByUser / DisabledByPolicy); the returned state is the truth.
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::ApplicationModel::StartupTaskState> SetEnabledAsync(bool enabled);
    };
}
