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

        bool Push(const T &data)
        {
            uint32_t tail = m_tail.load(std::memory_order_relaxed);
            uint32_t nextTail = (tail + 1) % N;

            if (!IsFull())
            {
                m_data[tail] = data;
                m_tail.store(nextTail, std::memory_order_release);

                return true;
            }

            return false;
        }

        bool Pop(T &data)
        {
            uint32_t head = m_head.load(std::memory_order_relaxed);

            if (IsEmpty())
            {
                // Queue is empty
                return false;
            }

            data = m_data[head];
            m_head.store((head + 1) % N, std::memory_order_release);

            return true;
        }

        bool Peek(T &data)
        {
            uint32_t head = m_head.load(std::memory_order_relaxed);

            if (IsEmpty())
            {
                // Queue is empty
                return false;
            }

            data = m_data[head];

            return true;
        }

        void Dequeue()
        {
            if (IsEmpty())
            {
                return;
            }

            m_head.store((m_head.load(std::memory_order_relaxed) + 1) % N, std::memory_order_release);
        }

        void Clear()
        {
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
        }

        bool IsEmpty() const
        {
            return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
        }

        bool IsFull() const
        {
            uint32_t tail = m_tail.load(std::memory_order_acquire);
            uint32_t nextTail = (tail + 1) % N;

            return nextTail == m_head.load(std::memory_order_acquire);
        }
    };

}