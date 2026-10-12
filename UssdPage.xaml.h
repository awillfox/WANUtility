#pragma once

#include "UssdPage.g.h"

namespace winrt::WANUtility::implementation
{
    struct UssdPage : UssdPageT<UssdPage>
    {
        UssdPage() {}
    };
}

namespace winrt::WANUtility::factory_implementation
{
    struct UssdPage : UssdPageT<UssdPage, implementation::UssdPage>
    {
    };
}
