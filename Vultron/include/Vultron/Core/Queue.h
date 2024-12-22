#pragma once

#include <array>
#include <atomic>

namespace Vultron
{
    template <typename T, size_t N>
    class Queue
    {
    private:
        std::array<T, N> m_data;
        std::atomic<uint32_t> m_head = 0;
        std::atomic<uint32_t> m_tail = 0;

    public:
        Queue() = default;
        ~Queue() = default;

        // Single Producer Only
        bool Push(const T &item)
        {
            // Read the tail index (current write position).
            // Relaxed is fine for reading and arithmetic.
            const uint32_t tail = m_tail.load(std::memory_order_relaxed);
            const uint32_t nextTail = (tail + 1) % N;

            // Acquire ensures we see the latest 'm_head' from Pop()
            // before deciding if it's full.
            const uint32_t head = m_head.load(std::memory_order_acquire);
            if (nextTail == head)
            {
                // Queue is full
                return false;
            }

            // Place the item
            m_data[tail] = item;
            // Release ensures the item is fully written before updating 'm_tail'.
            m_tail.store(nextTail, std::memory_order_release);
            return true;
        }

        // Single Consumer Only
        bool Pop(T &out)
        {
            // Read the head index (current read position).
            // Relaxed is fine for reading and arithmetic.
            const uint32_t head = m_head.load(std::memory_order_relaxed);

            // Acquire ensures we see the latest 'm_tail' from Push().
            const uint32_t tail = m_tail.load(std::memory_order_acquire);
            if (head == tail)
            {
                // Queue is empty
                return false;
            }

            // Read the item
            out = m_data[head];
            // Release ensures we’re done reading 'm_data[head]' before moving 'm_head'.
            m_head.store((head + 1) % N, std::memory_order_release);
            return true;
        }

        // Optional utility methods:

        bool IsEmpty() const
        {
            // Acquire needed to see up-to-date values from the other thread
            return m_head.load(std::memory_order_acquire) ==
                   m_tail.load(std::memory_order_acquire);
        }

        bool IsFull() const
        {
            const uint32_t tail = m_tail.load(std::memory_order_acquire);
            const uint32_t nextTail = (tail + 1) % N;
            const uint32_t head = m_head.load(std::memory_order_acquire);
            return (nextTail == head);
        }

        void Clear()
        {
            // Safe only if called when no Push/Pop in progress
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
        }
    };

}