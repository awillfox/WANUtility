#pragma once
#include <functional>
#include <string>
#include <vector>

namespace wan::core
{
    struct StoreEntry
    {
        std::wstring id;
        bool incoming = false;
    };

    // Scans one newest-first batch read from the Windows chat store.
    // - Unknown incoming entries are appended to `fresh` (newest first).
    // - Outgoing entries are skipped, known or not.
    // - Returns true at the first KNOWN INCOMING entry: everything older was imported before.
    // A known OUTGOING entry must not stop the scan: a message we just synced into the store can be
    // newer than incoming messages that have not been imported yet.
    bool ScanBatch(std::vector<StoreEntry> const& batch,
                   std::function<bool(std::wstring const&)> const& isKnown,
                   std::vector<StoreEntry>& fresh);
}
