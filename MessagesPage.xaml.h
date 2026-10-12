#pragma once

#include "MessagesPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct MessagesPage : MessagesPageT<MessagesPage>
    {
        MessagesPage() {}
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct MessagesPage : MessagesPageT<MessagesPage, implementation::MessagesPage>
    {
    };
}
