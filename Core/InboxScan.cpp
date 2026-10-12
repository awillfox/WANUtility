#include "InboxScan.h"

namespace wan::core
{
    bool ScanBatch(std::vector<StoreEntry> const& batch,
                   std::function<bool(std::wstring const&)> const& isKnown,
                   std::vector<StoreEntry>& fresh)
    {
        for (auto const& entry : batch)
        {
            if (!entry.incoming)
            {
                continue;
            }
            if (isKnown(entry.id))
            {
                return true;
            }
            fresh.push_back(entry);
        }
        return false;
    }
}
