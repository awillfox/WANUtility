#include "CppUnitTest.h"
#include "InboxScan.h"

#include <set>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

namespace
{
    auto KnownSet(std::set<std::wstring> ids)
    {
        return [ids](std::wstring const& id) { return ids.contains(id); };
    }
}

TEST_CLASS(InboxScanTests)
{
public:
    TEST_METHOD(CollectsUnknownIncomingNewestFirst)
    {
        std::vector<StoreEntry> fresh;
        int run = 0;
        bool stop = ScanBatch({ { L"c", true }, { L"b", true }, { L"a", true } }, KnownSet({}), fresh, run, 1);
        Assert::IsFalse(stop);
        Assert::AreEqual(3, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"c"), fresh[0].id);
    }

    TEST_METHOD(StopsAfterRunOfKnownIncoming)
    {
        std::vector<StoreEntry> fresh;
        int run = 0;
        bool stop = ScanBatch({ { L"c", true }, { L"b", true }, { L"a", true } }, KnownSet({ L"b", L"a" }), fresh, run, 2);
        Assert::IsTrue(stop);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"c"), fresh[0].id);
    }

    TEST_METHOD(UnknownIncomingBelowAKnownOneIsStillCollected)
    {
        // "late" was delivered after "a" was imported but sorts below it (e.g. queued at the SMSC).
        std::vector<StoreEntry> fresh;
        int run = 0;
        bool stop = ScanBatch({ { L"a", true }, { L"late", true }, { L"old1", true }, { L"old2", true } },
                              KnownSet({ L"a", L"old1", L"old2" }), fresh, run, 2);
        Assert::IsTrue(stop);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"late"), fresh[0].id);
    }

    TEST_METHOD(KnownRunCarriesAcrossBatches)
    {
        std::vector<StoreEntry> fresh;
        int run = 0;
        Assert::IsFalse(ScanBatch({ { L"x", true }, { L"a", true } }, KnownSet({ L"a", L"b" }), fresh, run, 2));
        Assert::IsTrue(ScanBatch({ { L"b", true }, { L"y", true } }, KnownSet({ L"a", L"b" }), fresh, run, 2));
        Assert::AreEqual(1, static_cast<int>(fresh.size())); // y is below the stop point
    }

    TEST_METHOD(KnownOutgoingDoesNotStopTheScan)
    {
        // We synced outgoing "out1" after incoming "new1" arrived but before it was imported.
        std::vector<StoreEntry> fresh;
        int run = 0;
        bool stop = ScanBatch({ { L"out1", false }, { L"new1", true }, { L"old1", true } }, KnownSet({ L"out1", L"old1" }), fresh, run, 1);
        Assert::IsTrue(stop);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"new1"), fresh[0].id);
    }

    TEST_METHOD(UnknownOutgoingIsSkipped)
    {
        std::vector<StoreEntry> fresh;
        int run = 0;
        ScanBatch({ { L"x", false }, { L"y", true } }, KnownSet({}), fresh, run, 1);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"y"), fresh[0].id);
    }

    TEST_METHOD(EmptyBatchDoesNotStop)
    {
        std::vector<StoreEntry> fresh;
        int run = 0;
        Assert::IsFalse(ScanBatch({}, KnownSet({}), fresh, run, 1));
        Assert::AreEqual(0, static_cast<int>(fresh.size()));
    }
};
