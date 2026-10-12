#pragma once

#include "SettingsPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage() {}
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
