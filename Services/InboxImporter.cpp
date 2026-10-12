#include "pch.h"
#include "Services/InboxImporter.h"
#include "Services/AppServices.h"
#include "Services/ErrorText.h"
#include "Services/TimeUtil.h"
#include "Core/InboxScan.h"
#include "Core/Log.h"

#include <unordered_map>

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Chat;

namespace wan
{
    winrt::fire_and_forget InboxImporter::Start()
    {
        try
        {
            m_store = co_await ChatMessageManager::RequestStoreAsync();
        }
        catch (hresult_error const& e)
        {
            ReportError(L"Cannot read the Windows message store: " + DescribeError(e));
            co_return;
        }
        if (!m_store)
        {
            ReportError(L"Windows returned no message store.");
            co_return;
        }

        // Fires on a worker thread and also without new messages (observed during USSD): just re-import.
        m_store.MessageChanged([](ChatMessageStore const&, ChatMessageChangedEventArgs const&)
        {
            Services().dispatcher.TryEnqueue([] { Services().importer->Import(); });
        });
        // Safety net: MessageChanged was never observed for a genuinely new SMS in the spike.
        m_timer = Services().dispatcher.CreateTimer();
        m_timer.Interval(std::chrono::seconds(60));
        m_timer.Tick([](auto const&, auto const&) { Services().importer->Import(); });
        m_timer.Start();

        co_await ImportAsync();
    }

    winrt::fire_and_forget InboxImporter::Import()
    {
        co_await ImportAsync();
    }

    Windows::Foundation::IAsyncAction InboxImporter::ImportAsync()
    {
        if (!m_store || !Services().db)
        {
            co_return;
        }
        if (m_running)
        {
            m_again = true; // coalesce bursts of triggers into one more pass
            co_return;
        }
        m_running = true;
        do
        {
            m_again = false;
            try
            {
                auto& db = *Services().db;
                const bool firstImport = !db.GetSetting(L"first_import_done").has_value();
                auto isKnown = [&db](std::wstring const& id) { return db.HasWindowsStoreId(id); };

                std::vector<core::StoreEntry> fresh;
                std::unordered_map<std::wstring, ChatMessage> byId;
                auto reader = m_store.GetMessageReader();
                while (true)
                {
                    auto batch = co_await reader.ReadBatchAsync(); // newest first (observed)
                    if (batch.Size() == 0)
                    {
                        break;
                    }
                    std::vector<core::StoreEntry> entries;
                    for (auto const& m : batch)
                    {
                        std::wstring id{ m.Id() };
                        entries.push_back({ id, m.IsIncoming() });
                        byId.emplace(id, m);
                    }
                    if (core::ScanBatch(entries, isKnown, fresh))
                    {
                        break;
                    }
                }

                int inserted = 0;
                for (auto it = fresh.rbegin(); it != fresh.rend(); ++it) // oldest first
                {
                    auto const& m = byId.at(it->id);
                    const auto rowId = db.InsertIncoming(std::wstring(m.From()), std::wstring(m.Body()), ToUnixMs(m.LocalTimestamp()), it->id);
                    if (rowId == 0)
                    {
                        continue;
                    }
                    ++inserted;
                    if (!firstImport)
                    {
                        if (auto saved = db.MessageById(rowId))
                        {
                            NewIncoming.Raise(*saved);
                        }
                    }
                }
                if (firstImport)
                {
                    db.SetSetting(L"first_import_done", L"1");
                }
                if (inserted > 0)
                {
                    core::Log::Write(L"Imported " + std::to_wstring(inserted) + L" message(s)" + (firstImport ? L" (first import, no toasts)" : L""));
                    Services().MessagesChanged.Raise(L"");
                }
            }
            catch (hresult_error const& e)
            {
                ReportError(L"Importing messages failed: " + DescribeError(e));
            }
            catch (core::DbError const& e)
            {
                ReportError(L"Saving imported messages failed: " + Widen(e.what()));
            }
        } while (m_again);
        m_running = false;
    }
}
