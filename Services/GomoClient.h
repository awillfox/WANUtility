#pragma once
#include "Core/GomoAccount.h"

namespace wan
{
    // Read-only GOMO account fetch over the current cellular connection. Blocking: call off the UI thread.
    // The carrier identifies the subscriber from the cellular connection (no OTP). The session token lives
    // only inside one call. Throws core::GomoError.
    core::GomoAccount FetchGomoAccount();
}
