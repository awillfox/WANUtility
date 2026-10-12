#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct sqlite3;

namespace wan::core
{
    enum class Direction : int { In = 0, Out = 1 };
    enum class MessageStatus : int { Sending = 0, Sent = 1, Failed = 2, Received = 3 };
    enum class SyncState : int { NotApplicable = 0, Pending = 1, Synced = 2, Disabled = 3 };

    struct Message
    {
        int64_t id = 0;
        std::wstring threadKey;
        Direction direction = Direction::In;
        std::wstring address;       // sender (in) or recipient (out), as given
        std::wstring body;
        int64_t timestampMs = 0;    // Unix epoch, milliseconds, UTC
        MessageStatus status = MessageStatus::Received;
        std::wstring errorDetail;
        std::wstring windowsStoreId;
        SyncState syncState = SyncState::NotApplicable;
        bool isRead = false;
    };

    struct Thread
    {
        std::wstring threadKey;
        std::wstring displayName;   // address of the newest message
        int64_t lastMessageAtMs = 0;
        std::wstring lastBody;
        int unreadCount = 0;
    };

    struct UssdCode
    {
        int64_t id = 0;
        std::wstring code;
        std::wstring label;
        int sortOrder = 0;
    };

    struct UssdHistoryEntry
    {
        int64_t id = 0;
        std::wstring code;
        std::wstring finalReply;
        int64_t timestampMs = 0;
    };

    class DbError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // Single-connection SQLite store. Not thread-safe: use from the UI thread only.
    class MessageDb
    {
    public:
        explicit MessageDb(std::wstring const& path);
        ~MessageDb();
        MessageDb(MessageDb const&) = delete;
        MessageDb& operator=(MessageDb const&) = delete;

        bool HasWindowsStoreId(std::wstring const& id);
        int64_t InsertIncoming(std::wstring const& address, std::wstring const& body, int64_t timestampMs, std::wstring const& windowsStoreId);
        int64_t InsertOutgoing(std::wstring const& address, std::wstring const& body, int64_t timestampMs);
        void MarkSending(int64_t id);
        void MarkSent(int64_t id);
        void MarkFailed(int64_t id, std::wstring const& detail);
        void MarkSynced(int64_t id, std::wstring const& windowsStoreId);
        void DisableSync(std::wstring const& reason);
        std::vector<Message> PendingSync();
        std::optional<Message> MessageById(int64_t id);

        std::vector<Thread> Threads(std::wstring const& search = L"");
        std::vector<Message> MessagesInThread(std::wstring const& threadKey);
        int MarkThreadRead(std::wstring const& threadKey);
        // Marks rows left in "sending" by an earlier run as failed (never re-sends). Returns rows changed.
        int FailInterruptedSends(std::wstring const& detail);

        std::vector<UssdCode> UssdCodes();
        int64_t AddUssdCode(std::wstring const& code, std::wstring const& label);
        void RenameUssdCode(int64_t id, std::wstring const& label);
        void RemoveUssdCode(int64_t id);
        void AddUssdHistory(std::wstring const& code, std::wstring const& finalReply, int64_t timestampMs);
        std::vector<UssdHistoryEntry> RecentUssd(int limit);

        std::optional<std::wstring> GetSetting(std::wstring const& key);
        void SetSetting(std::wstring const& key, std::wstring const& value);

    private:
        sqlite3* m_db = nullptr;
    };
}
