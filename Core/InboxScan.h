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
    // - Outgoing entries are skipped, known or not, and do not affect the stop rule.
    // - Returns true once `stopAfterKnown` KNOWN INCOMING entries have been seen in a row
    //   (`knownRun` carries the count across batches; start it at 0). An unknown incoming entry
    //   resets the run, so a message that sorts below an already-imported one is still found.
    // A known OUTGOING entry must not stop the scan: a message we just synced into the store can be
    // newer than incoming messages that have not been imported yet.
    bool ScanBatch(std::vector<StoreEntry> const& batch,
                   std::function<bool(std::wstring const&)> const& isKnown,
                   std::vector<StoreEntry>& fresh,
                   int& knownRun,
                   int stopAfterKnown);
}
