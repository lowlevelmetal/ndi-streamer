/**
 * @file queue.hpp
 * @brief Bounded, closable, multi-producer/multi-consumer blocking queue.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>

namespace ndistreamer {

/**
 * @brief A bounded FIFO that connects pipeline stages.
 *
 * Producers block while the queue is full, which provides back-pressure. Consumers block while it is
 * empty. Close() marks the end of the stream: pending items can still be drained, after which Pop()
 * returns std::nullopt. Every blocking call also returns early when its stop token is triggered.
 */
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : m_capacity(capacity == 0 ? 1 : capacity) {}

    BoundedQueue(const BoundedQueue &) = delete;
    BoundedQueue &operator=(const BoundedQueue &) = delete;

    /**
     * @brief Append an item, waiting for space if necessary.
     * @return false if the queue was closed or a stop was requested; the item is discarded.
     */
    bool Push(T item, std::stop_token stop) {
        std::unique_lock lock(m_mutex);
        if (!m_not_full.wait(lock, stop, [this] { return m_closed || m_items.size() < m_capacity; })) {
            return false;
        }
        if (m_closed) return false;

        m_items.push_back(std::move(item));
        lock.unlock();
        m_not_empty.notify_one();
        return true;
    }

    enum class PushResult { Pushed, TimedOut, Closed };

    /**
     * @brief Append an item, waiting at most @p timeout for space.
     * @p item is moved from only when the result is Pushed. Closed also covers stop requests.
     */
    template <typename Rep, typename Period>
    PushResult PushFor(T &item, std::chrono::duration<Rep, Period> timeout, std::stop_token stop) {
        std::unique_lock lock(m_mutex);
        bool ready = m_not_full.wait_for(lock, stop, timeout,
                                         [this] { return m_closed || m_items.size() < m_capacity; });
        if (m_closed || stop.stop_requested()) return PushResult::Closed;
        if (!ready) return PushResult::TimedOut;

        m_items.push_back(std::move(item));
        lock.unlock();
        m_not_empty.notify_one();
        return PushResult::Pushed;
    }

    /**
     * @brief Remove the oldest item, waiting for one if necessary.
     * @return std::nullopt once the queue is closed and drained, or if a stop was requested.
     */
    std::optional<T> Pop(std::stop_token stop) {
        std::unique_lock lock(m_mutex);
        if (!m_not_empty.wait(lock, stop, [this] { return m_closed || !m_items.empty(); })) {
            return std::nullopt;
        }
        if (m_items.empty()) return std::nullopt;

        T item = std::move(m_items.front());
        m_items.pop_front();
        lock.unlock();
        m_not_full.notify_one();
        return item;
    }

    /**
     * @brief Signal that no more items will be pushed. Wakes all waiters.
     */
    void Close() {
        {
            std::lock_guard lock(m_mutex);
            m_closed = true;
        }
        m_not_empty.notify_all();
        m_not_full.notify_all();
    }

    std::size_t Size() const {
        std::lock_guard lock(m_mutex);
        return m_items.size();
    }

    std::size_t Capacity() const { return m_capacity; }

private:
    mutable std::mutex m_mutex;
    std::condition_variable_any m_not_empty;
    std::condition_variable_any m_not_full;
    std::deque<T> m_items;
    const std::size_t m_capacity;
    bool m_closed = false;
};

} // namespace ndistreamer
