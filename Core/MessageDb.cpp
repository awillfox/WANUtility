#include "MessageDb.h"
#include "PhoneNumber.h"

#include <winsqlite/winsqlite3.h>

namespace wan::core
{
    namespace
    {
        constexpr const char* kSchema = R"SQL(
            CREATE TABLE IF NOT EXISTS messages(
                id               INTEGER PRIMARY KEY AUTOINCREMENT,
                thread_key       TEXT    NOT NULL,
                direction        INTEGER NOT NULL,
                address          TEXT    NOT NULL,
                body             TEXT    NOT NULL,
                timestamp_ms     INTEGER NOT NULL,
                status           INTEGER NOT NULL,
                error_detail     TEXT    NOT NULL DEFAULT '',
                windows_store_id TEXT    UNIQUE,
                sync_state       INTEGER NOT NULL,
                is_read          INTEGER NOT NULL);
            CREATE INDEX IF NOT EXISTS ix_messages_thread ON messages(thread_key, timestamp_ms);
            CREATE TABLE IF NOT EXISTS ussd_codes(
                id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT NOT NULL, label TEXT NOT NULL, sort_order INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS ussd_history(
                id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT NOT NULL, final_reply TEXT NOT NULL, timestamp_ms INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT NOT NULL);
        )SQL";

        constexpr const wchar_t* kMessageColumns =
            L"id, thread_key, direction, address, body, timestamp_ms, status, error_detail, "
            L"COALESCE(windows_store_id, ''), sync_state, is_read";

        // RAII prepared statement using the UTF-16 sqlite API (wchar_t is UTF-16 on Windows).
        class Stmt
        {
        public:
            Stmt(sqlite3* db, std::wstring const& sql) : m_db(db)
            {
                if (sqlite3_prepare16_v2(db, sql.c_str(), -1, &m_stmt, nullptr) != SQLITE_OK)
                {
                    throw DbError(std::string("prepare: ") + sqlite3_errmsg(db));
                }
            }
            ~Stmt() { sqlite3_finalize(m_stmt); }
            Stmt(Stmt const&) = delete;
            Stmt& operator=(Stmt const&) = delete;

            Stmt& Bind(int index, int64_t value) { Check(sqlite3_bind_int64(m_stmt, index, value)); return *this; }
            Stmt& Bind(int index, std::wstring const& value)
            {
                // c_str() is never null, so an empty string binds '' (not NULL).
                Check(sqlite3_bind_text16(m_stmt, index, value.c_str(), static_cast<int>(value.size() * sizeof(wchar_t)), SQLITE_TRANSIENT));
                return *this;
            }
            Stmt& BindNullable(int index, std::wstring const& value)
            {
                if (value.empty()) { Check(sqlite3_bind_null(m_stmt, index)); return *this; }
                return Bind(index, value);
            }

            bool Step()
            {
                const int rc = sqlite3_step(m_stmt);
                if (rc == SQLITE_ROW) return true;
                if (rc == SQLITE_DONE) return false;
                throw DbError(std::string("step: ") + sqlite3_errmsg(m_db));
            }
            void Run() { while (Step()) {} }

            int64_t Int(int col) { return sqlite3_column_int64(m_stmt, col); }
            std::wstring Text(int col)
            {
                auto p = static_cast<const wchar_t*>(sqlite3_column_text16(m_stmt, col));
                if (!p) return {};
                return std::wstring(p, sqlite3_column_bytes16(m_stmt, col) / sizeof(wchar_t));
            }

        private:
            void Check(int rc)
            {
                if (rc != SQLITE_OK) throw DbError(std::string("bind: ") + sqlite3_errmsg(m_db));
            }
            sqlite3* m_db;
            sqlite3_stmt* m_stmt = nullptr;
        };

        void Exec(sqlite3* db, const char* sql)
        {
            char* err = nullptr;
            if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK)
            {
                std::string message = err ? err : "unknown error";
                sqlite3_free(err);
                throw DbError("exec: " + message);
            }
        }

        Message ReadMessage(Stmt& s)
        {
            Message m;
            m.id = s.Int(0);
            m.threadKey = s.Text(1);
            m.direction = static_cast<Direction>(s.Int(2));
            m.address = s.Text(3);
            m.body = s.Text(4);
            m.timestampMs = s.Int(5);
            m.status = static_cast<MessageStatus>(s.Int(6));
            m.errorDetail = s.Text(7);
            m.windowsStoreId = s.Text(8);
            m.syncState = static_cast<SyncState>(s.Int(9));
            m.isRead = s.Int(10) != 0;
            return m;
        }

        std::wstring LikePattern(std::wstring const& search)
        {
            std::wstring p = L"%";
            for (wchar_t c : search)
            {
                if (c == L'%' || c == L'_' || c == L'\\') p.push_back(L'\\');
                p.push_back(c);
            }
            p.push_back(L'%');
            return p;
        }

        int64_t I(auto e) { return static_cast<int64_t>(e); }
    }

    MessageDb::MessageDb(std::wstring const& path)
    {
        if (sqlite3_open16(path.c_str(), &m_db) != SQLITE_OK)
        {
            std::string message = m_db ? sqlite3_errmsg(m_db) : "out of memory";
            sqlite3_close(m_db);
            m_db = nullptr;
            throw DbError("open: " + message);
        }
        try
        {
            Exec(m_db, kSchema);
            int64_t version = 0;
            {
                Stmt v(m_db, L"PRAGMA user_version;");
                if (v.Step()) version = v.Int(0);
            }
            if (version == 0)
            {
                AddUssdCode(L"*101#", L"Balance");
                AddUssdCode(L"*103#", L"Packages");
                AddUssdCode(L"*101*1#", L"Status SMS");
                Exec(m_db, "PRAGMA user_version = 1;");
            }
        }
        catch (...)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
            throw;
        }
    }

    MessageDb::~MessageDb()
    {
        sqlite3_close(m_db);
    }

    bool MessageDb::HasWindowsStoreId(std::wstring const& id)
    {
        Stmt s(m_db, L"SELECT 1 FROM messages WHERE windows_store_id = ?1;");
        s.Bind(1, id);
        return s.Step();
    }

    int64_t MessageDb::InsertIncoming(std::wstring const& address, std::wstring const& body, int64_t timestampMs, std::wstring const& windowsStoreId)
    {
        Stmt s(m_db, L"INSERT OR IGNORE INTO messages(thread_key, direction, address, body, timestamp_ms, status, windows_store_id, sync_state, is_read) "
                     L"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 0);");
        s.Bind(1, ThreadKeyFor(address)).Bind(2, I(Direction::In)).Bind(3, address).Bind(4, body).Bind(5, timestampMs)
         .Bind(6, I(MessageStatus::Received)).BindNullable(7, windowsStoreId).Bind(8, I(SyncState::NotApplicable));
        s.Run();
        return sqlite3_changes(m_db) == 0 ? 0 : sqlite3_last_insert_rowid(m_db);
    }

    int64_t MessageDb::InsertOutgoing(std::wstring const& address, std::wstring const& body, int64_t timestampMs)
    {
        Stmt s(m_db, L"INSERT INTO messages(thread_key, direction, address, body, timestamp_ms, status, sync_state, is_read) "
                     L"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, 1);");
        s.Bind(1, ThreadKeyFor(address)).Bind(2, I(Direction::Out)).Bind(3, address).Bind(4, body).Bind(5, timestampMs)
         .Bind(6, I(MessageStatus::Sending)).Bind(7, I(SyncState::NotApplicable));
        s.Run();
        return sqlite3_last_insert_rowid(m_db);
    }

    void MessageDb::MarkSending(int64_t id)
    {
        Stmt s(m_db, L"UPDATE messages SET status = ?1, error_detail = '' WHERE id = ?2;");
        s.Bind(1, I(MessageStatus::Sending)).Bind(2, id).Run();
    }

    void MessageDb::MarkSent(int64_t id)
    {
        const bool syncOn = GetSetting(L"store_sync_enabled").value_or(L"1") != L"0";
        Stmt s(m_db, L"UPDATE messages SET status = ?1, error_detail = '', sync_state = ?2 WHERE id = ?3;");
        s.Bind(1, I(MessageStatus::Sent)).Bind(2, I(syncOn ? SyncState::Pending : SyncState::Disabled)).Bind(3, id).Run();
    }

    void MessageDb::MarkFailed(int64_t id, std::wstring const& detail)
    {
        Stmt s(m_db, L"UPDATE messages SET status = ?1, error_detail = ?2 WHERE id = ?3;");
        s.Bind(1, I(MessageStatus::Failed)).Bind(2, detail).Bind(3, id).Run();
    }

    void MessageDb::MarkSynced(int64_t id, std::wstring const& windowsStoreId)
    {
        Stmt s(m_db, L"UPDATE messages SET sync_state = ?1, windows_store_id = ?2 WHERE id = ?3;");
        s.Bind(1, I(SyncState::Synced)).BindNullable(2, windowsStoreId).Bind(3, id).Run();
    }

    void MessageDb::DisableSync(std::wstring const& reason)
    {
        SetSetting(L"store_sync_enabled", L"0");
        SetSetting(L"store_sync_disabled_reason", reason);
        Stmt s(m_db, L"UPDATE messages SET sync_state = ?1 WHERE sync_state = ?2;");
        s.Bind(1, I(SyncState::Disabled)).Bind(2, I(SyncState::Pending)).Run();
    }

    std::vector<Message> MessageDb::PendingSync()
    {
        Stmt s(m_db, std::wstring(L"SELECT ") + kMessageColumns + L" FROM messages WHERE sync_state = ?1 AND status = ?2 ORDER BY id;");
        s.Bind(1, I(SyncState::Pending)).Bind(2, I(MessageStatus::Sent));
        std::vector<Message> out;
        while (s.Step()) out.push_back(ReadMessage(s));
        return out;
    }

    std::optional<Message> MessageDb::MessageById(int64_t id)
    {
        Stmt s(m_db, std::wstring(L"SELECT ") + kMessageColumns + L" FROM messages WHERE id = ?1;");
        s.Bind(1, id);
        if (!s.Step()) return std::nullopt;
        return ReadMessage(s);
    }

    std::vector<Thread> MessageDb::Threads(std::wstring const& search)
    {
        Stmt s(m_db, LR"SQL(
            SELECT m.thread_key, m.address, m.timestamp_ms, m.body,
                   (SELECT COUNT(*) FROM messages u WHERE u.thread_key = m.thread_key AND u.direction = 0 AND u.is_read = 0)
            FROM messages m
            WHERE m.id = (SELECT x.id FROM messages x WHERE x.thread_key = m.thread_key ORDER BY x.timestamp_ms DESC, x.id DESC LIMIT 1)
              AND (?1 = '' OR m.thread_key IN (
                    SELECT f.thread_key FROM messages f
                    WHERE f.body LIKE ?2 ESCAPE '\' OR f.address LIKE ?2 ESCAPE '\' OR f.thread_key LIKE ?2 ESCAPE '\'))
            ORDER BY m.timestamp_ms DESC, m.id DESC;)SQL");
        s.Bind(1, search).Bind(2, LikePattern(search));
        std::vector<Thread> out;
        while (s.Step())
        {
            out.push_back(Thread{ s.Text(0), s.Text(1), s.Int(2), s.Text(3), static_cast<int>(s.Int(4)) });
        }
        return out;
    }

    std::vector<Message> MessageDb::MessagesInThread(std::wstring const& threadKey)
    {
        Stmt s(m_db, std::wstring(L"SELECT ") + kMessageColumns + L" FROM messages WHERE thread_key = ?1 ORDER BY timestamp_ms, id;");
        s.Bind(1, threadKey);
        std::vector<Message> out;
        while (s.Step()) out.push_back(ReadMessage(s));
        return out;
    }

    int MessageDb::MarkThreadRead(std::wstring const& threadKey)
    {
        Stmt s(m_db, L"UPDATE messages SET is_read = 1 WHERE thread_key = ?1 AND is_read = 0;");
        s.Bind(1, threadKey).Run();
        return sqlite3_changes(m_db);
    }

    int MessageDb::FailInterruptedSends(std::wstring const& detail)
    {
        Stmt s(m_db, L"UPDATE messages SET status = ?1, error_detail = ?2 WHERE direction = ?3 AND status = ?4;");
        s.Bind(1, I(MessageStatus::Failed)).Bind(2, detail).Bind(3, I(Direction::Out)).Bind(4, I(MessageStatus::Sending)).Run();
        return sqlite3_changes(m_db);
    }

    std::vector<UssdCode> MessageDb::UssdCodes()
    {
        Stmt s(m_db, L"SELECT id, code, label, sort_order FROM ussd_codes ORDER BY sort_order, id;");
        std::vector<UssdCode> out;
        while (s.Step()) out.push_back(UssdCode{ s.Int(0), s.Text(1), s.Text(2), static_cast<int>(s.Int(3)) });
        return out;
    }

    int64_t MessageDb::AddUssdCode(std::wstring const& code, std::wstring const& label)
    {
        Stmt s(m_db, L"INSERT INTO ussd_codes(code, label, sort_order) VALUES(?1, ?2, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM ussd_codes));");
        s.Bind(1, code).Bind(2, label).Run();
        return sqlite3_last_insert_rowid(m_db);
    }

    void MessageDb::RenameUssdCode(int64_t id, std::wstring const& label)
    {
        Stmt s(m_db, L"UPDATE ussd_codes SET label = ?1 WHERE id = ?2;");
        s.Bind(1, label).Bind(2, id).Run();
    }

    void MessageDb::RemoveUssdCode(int64_t id)
    {
        Stmt s(m_db, L"DELETE FROM ussd_codes WHERE id = ?1;");
        s.Bind(1, id).Run();
    }

    void MessageDb::AddUssdHistory(std::wstring const& code, std::wstring const& finalReply, int64_t timestampMs)
    {
        Stmt s(m_db, L"INSERT INTO ussd_history(code, final_reply, timestamp_ms) VALUES(?1, ?2, ?3);");
        s.Bind(1, code).Bind(2, finalReply).Bind(3, timestampMs).Run();
    }

    std::vector<UssdHistoryEntry> MessageDb::RecentUssd(int limit)
    {
        Stmt s(m_db, L"SELECT id, code, final_reply, timestamp_ms FROM ussd_history ORDER BY timestamp_ms DESC, id DESC LIMIT ?1;");
        s.Bind(1, static_cast<int64_t>(limit));
        std::vector<UssdHistoryEntry> out;
        while (s.Step()) out.push_back(UssdHistoryEntry{ s.Int(0), s.Text(1), s.Text(2), s.Int(3) });
        return out;
    }

    std::optional<std::wstring> MessageDb::GetSetting(std::wstring const& key)
    {
        Stmt s(m_db, L"SELECT value FROM settings WHERE key = ?1;");
        s.Bind(1, key);
        if (!s.Step()) return std::nullopt;
        return s.Text(0);
    }

    void MessageDb::SetSetting(std::wstring const& key, std::wstring const& value)
    {
        Stmt s(m_db, L"INSERT INTO settings(key, value) VALUES(?1, ?2) ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
        s.Bind(1, key).Bind(2, value).Run();
    }
}
