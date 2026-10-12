#pragma once
#include <functional>
#include <utility>
#include <vector>

namespace wan
{
    // Minimal observer list for service -> UI notifications. UI thread only.
    // Raise() iterates a copy, so handlers may Add/Remove while being raised.
    template <typename... Args>
    class Signal
    {
    public:
        using Handler = std::function<void(Args const&...)>;

        int Add(Handler handler)
        {
            m_handlers.emplace_back(++m_next, std::move(handler));
            return m_next;
        }

        void Remove(int token)
        {
            std::erase_if(m_handlers, [token](auto const& entry) { return entry.first == token; });
        }

        void Raise(Args const&... args)
        {
            auto copy = m_handlers;
            for (auto& [token, handler] : copy)
            {
                handler(args...);
            }
        }

    private:
        std::vector<std::pair<int, Handler>> m_handlers;
        int m_next = 0;
    };
}
