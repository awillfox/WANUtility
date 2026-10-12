#include "CppUnitTest.h"
#include "MessageDb.h"

#include <filesystem>
#include <windows.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace wan::core;

TEST_CLASS(MessageDbTests)
{
    std::filesystem::path m_path;

    static std::filesystem::path NewTempPath()
    {
        static int counter = 0;
        return std::filesystem::temp_directory_path() /
            (L"wanutility_test_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(++counter) + L".db");
    }

public:
    TEST_METHOD_INITIALIZE(Init)
    {
        m_path = NewTempPath();
        std::filesystem::remove(m_path);
    }

    TEST_METHOD_CLEANUP(Cleanup)
    {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

    TEST_METHOD(NewDatabaseSeedsThreeUssdCodesOnce)
    {
        {
            MessageDb db(m_path.wstring());
            auto codes = db.UssdCodes();
            Assert::AreEqual(3, static_cast<int>(codes.size()));
            Assert::AreEqual(std::wstring(L"*101#"), codes[0].code);
            Assert::AreEqual(std::wstring(L"Balance"), codes[0].label);
            Assert::AreEqual(std::wstring(L"*103#"), codes[1].code);
            Assert::AreEqual(std::wstring(L"*101*1#"), codes[2].code);
        }
        MessageDb reopened(m_path.wstring());
        Assert::AreEqual(3, static_cast<int>(reopened.UssdCodes().size()));
    }

    TEST_METHOD(DuplicateWindowsStoreIdIsIgnored)
    {
        MessageDb db(m_path.wstring());
        auto first = db.InsertIncoming(L"dtac", L"hello", 1000, L"store-1");
        auto second = db.InsertIncoming(L"dtac", L"hello", 1000, L"store-1");
        Assert::IsTrue(first > 0);
        Assert::IsTrue(second == 0);
        Assert::IsTrue(db.HasWindowsStoreId(L"store-1"));
        Assert::IsFalse(db.HasWindowsStoreId(L"store-2"));
        Assert::AreEqual(1, static_cast<int>(db.MessagesInThread(L"dtac").size()));
    }

    TEST_METHOD(LocalAndInternationalNumbersShareOneThread)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"+66889605613", L"hi", 1000, L"s1");
        db.InsertOutgoing(L"0889605613", L"hello back", 2000);
        auto threads = db.Threads();
        Assert::AreEqual(1, static_cast<int>(threads.size()));
        Assert::AreEqual(std::wstring(L"+66889605613"), threads[0].threadKey);
        Assert::AreEqual(std::wstring(L"0889605613"), threads[0].displayName); // newest message's address
        Assert::AreEqual(std::wstring(L"hello back"), threads[0].lastBody);
        Assert::AreEqual(2, static_cast<int>(db.MessagesInThread(L"+66889605613").size()));
    }

    TEST_METHOD(ThreadsAreNewestFirstWithUnreadCounts)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"dtac", L"old", 1000, L"a");
        db.InsertIncoming(L"dtacOnline", L"newer", 3000, L"b");
        db.InsertIncoming(L"dtac", L"newest dtac", 2000, L"c");
        auto threads = db.Threads();
        Assert::AreEqual(2, static_cast<int>(threads.size()));
        Assert::AreEqual(std::wstring(L"dtacOnline"), threads[0].threadKey);
        Assert::AreEqual(std::wstring(L"dtac"), threads[1].threadKey);
        Assert::AreEqual(2, threads[1].unreadCount);
        Assert::AreEqual(2, db.MarkThreadRead(L"dtac"));
        Assert::AreEqual(0, db.Threads()[1].unreadCount);
        Assert::AreEqual(0, db.MarkThreadRead(L"dtac"));
    }

    TEST_METHOD(MessagesInThreadAreOldestFirst)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"dtac", L"second", 2000, L"b");
        db.InsertIncoming(L"dtac", L"first", 1000, L"a");
        auto m = db.MessagesInThread(L"dtac");
        Assert::AreEqual(std::wstring(L"first"), m[0].body);
        Assert::AreEqual(std::wstring(L"second"), m[1].body);
    }

    TEST_METHOD(SearchMatchesBodyOrAddress)
    {
        MessageDb db(m_path.wstring());
        db.InsertIncoming(L"dtac", L"ยอดคงเหลือ 47.79", 1000, L"a");
        db.InsertIncoming(L"dtacOnline", L"promo", 2000, L"b");
        Assert::AreEqual(1, static_cast<int>(db.Threads(L"47.79").size()));
        Assert::AreEqual(2, static_cast<int>(db.Threads(L"dtac").size()));
        Assert::AreEqual(0, static_cast<int>(db.Threads(L"100%").size())); // % is literal, not a wildcard
    }

    TEST_METHOD(OutgoingLifecycleAndSync)
    {
        MessageDb db(m_path.wstring());
        auto id = db.InsertOutgoing(L"0889605613", L"test", 1000);
        Assert::IsTrue(db.MessageById(id)->status == MessageStatus::Sending);
        Assert::IsTrue(db.MessageById(id)->syncState == SyncState::NotApplicable);
        Assert::IsTrue(db.MessageById(id)->isRead);

        db.MarkFailed(id, L"Modem error 5");
        Assert::IsTrue(db.MessageById(id)->status == MessageStatus::Failed);
        Assert::AreEqual(std::wstring(L"Modem error 5"), db.MessageById(id)->errorDetail);
        Assert::AreEqual(0, static_cast<int>(db.PendingSync().size()));

        db.MarkSending(id);
        db.MarkSent(id);
        auto sent = *db.MessageById(id);
        Assert::IsTrue(sent.status == MessageStatus::Sent);
        Assert::AreEqual(std::wstring(), sent.errorDetail);
        Assert::IsTrue(sent.syncState == SyncState::Pending);
        Assert::AreEqual(1, static_cast<int>(db.PendingSync().size()));

        db.MarkSynced(id, L"store-out-1");
        Assert::IsTrue(db.MessageById(id)->syncState == SyncState::Synced);
        Assert::IsTrue(db.HasWindowsStoreId(L"store-out-1"));
        Assert::AreEqual(0, static_cast<int>(db.PendingSync().size()));
    }

    TEST_METHOD(DisableSyncStopsPendingAndFutureSync)
    {
        MessageDb db(m_path.wstring());
        auto a = db.InsertOutgoing(L"0889605613", L"a", 1000);
        db.MarkSent(a);
        db.DisableSync(L"Access denied");
        Assert::IsTrue(db.MessageById(a)->syncState == SyncState::Disabled);
        Assert::AreEqual(std::wstring(L"0"), *db.GetSetting(L"store_sync_enabled"));
        Assert::AreEqual(std::wstring(L"Access denied"), *db.GetSetting(L"store_sync_disabled_reason"));

        auto b = db.InsertOutgoing(L"0889605613", L"b", 2000);
        db.MarkSent(b);
        Assert::IsTrue(db.MessageById(b)->syncState == SyncState::Disabled);
        Assert::AreEqual(0, static_cast<int>(db.PendingSync().size()));
    }

    TEST_METHOD(MarkSyncedWithEmptyStoreIdKeepsColumnNull)
    {
        MessageDb db(m_path.wstring());
        auto a = db.InsertOutgoing(L"1678", L"a", 1000);
        auto b = db.InsertOutgoing(L"1678", L"b", 2000);
        db.MarkSent(a);
        db.MarkSent(b);
        db.MarkSynced(a, L"");
        db.MarkSynced(b, L""); // must not violate UNIQUE
        Assert::IsTrue(db.MessageById(b)->syncState == SyncState::Synced);
    }

    TEST_METHOD(UnicodeAndEmptyBodiesRoundTrip)
    {
        MessageDb db(m_path.wstring());
        auto id = db.InsertIncoming(L"dtac", L"", 1000, L"x");
        Assert::AreEqual(std::wstring(), db.MessageById(id)->body);
        auto id2 = db.InsertIncoming(L"dtac", L"คุณมีโปรโมชั่น 3GB", 2000, L"y");
        Assert::AreEqual(std::wstring(L"คุณมีโปรโมชั่น 3GB"), db.MessageById(id2)->body);
        Assert::IsFalse(db.MessageById(999).has_value());
    }

    TEST_METHOD(UssdCodesCanBeAddedRenamedRemoved)
    {
        MessageDb db(m_path.wstring());
        auto id = db.AddUssdCode(L"*121#", L"Promo");
        auto codes = db.UssdCodes();
        Assert::AreEqual(4, static_cast<int>(codes.size()));
        Assert::AreEqual(std::wstring(L"*121#"), codes[3].code);
        db.RenameUssdCode(id, L"Promotions");
        Assert::AreEqual(std::wstring(L"Promotions"), db.UssdCodes()[3].label);
        db.RemoveUssdCode(id);
        Assert::AreEqual(3, static_cast<int>(db.UssdCodes().size()));
    }

    TEST_METHOD(RecentUssdIsNewestFirstAndLimited)
    {
        MessageDb db(m_path.wstring());
        db.AddUssdHistory(L"*101#", L"one", 1000);
        db.AddUssdHistory(L"*101#", L"two", 2000);
        db.AddUssdHistory(L"*103#", L"three", 3000);
        auto recent = db.RecentUssd(2);
        Assert::AreEqual(2, static_cast<int>(recent.size()));
        Assert::AreEqual(std::wstring(L"three"), recent[0].finalReply);
        Assert::AreEqual(std::wstring(L"two"), recent[1].finalReply);
    }

    TEST_METHOD(SettingsOverwrite)
    {
        MessageDb db(m_path.wstring());
        Assert::IsFalse(db.GetSetting(L"first_import_done").has_value());
        db.SetSetting(L"first_import_done", L"1");
        db.SetSetting(L"first_import_done", L"2");
        Assert::AreEqual(std::wstring(L"2"), *db.GetSetting(L"first_import_done"));
    }

    TEST_METHOD(OpeningAnInvalidPathThrowsDbError)
    {
        auto bad = std::filesystem::temp_directory_path() / L"no_such_dir_wanutility" / L"x" / L"y.db";
        Assert::ExpectException<DbError>([&] { MessageDb db(bad.wstring()); });
    }
};
