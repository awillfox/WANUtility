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
        bool stop = ScanBatch({ { L"c", true }, { L"b", true }, { L"a", true } }, KnownSet({}), fresh);
        Assert::IsFalse(stop);
        Assert::AreEqual(3, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"c"), fresh[0].id);
    }

    TEST_METHOD(StopsAtFirstKnownIncoming)
    {
        std::vector<StoreEntry> fresh;
        bool stop = ScanBatch({ { L"c", true }, { L"b", true }, { L"a", true } }, KnownSet({ L"b" }), fresh);
        Assert::IsTrue(stop);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"c"), fresh[0].id);
    }

    TEST_METHOD(KnownOutgoingDoesNotStopTheScan)
    {
        // We synced outgoing "out1" after incoming "new1" arrived but before it was imported.
        std::vector<StoreEntry> fresh;
        bool stop = ScanBatch({ { L"out1", false }, { L"new1", true }, { L"old1", true } }, KnownSet({ L"out1", L"old1" }), fresh);
        Assert::IsTrue(stop);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"new1"), fresh[0].id);
    }

    TEST_METHOD(UnknownOutgoingIsSkipped)
    {
        std::vector<StoreEntry> fresh;
        ScanBatch({ { L"x", false }, { L"y", true } }, KnownSet({}), fresh);
        Assert::AreEqual(1, static_cast<int>(fresh.size()));
        Assert::AreEqual(std::wstring(L"y"), fresh[0].id);
    }

    TEST_METHOD(EmptyBatchDoesNotStop)
    {
        std::vector<StoreEntry> fresh;
        Assert::IsFalse(ScanBatch({}, KnownSet({}), fresh));
        Assert::AreEqual(0, static_cast<int>(fresh.size()));
    }
};
