#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <algorithm>

namespace RadiosondePI::DSP {

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : m_capacity(capacity)
        , m_buffer(capacity)
        , m_head(0)
        , m_tail(0) {}

    size_t write(const T* data, size_t count) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t tail = m_tail.load(std::memory_order_acquire);

        size_t available = (tail > head) ? (tail - head - 1) : (m_capacity - head + tail - 1);
        size_t toWrite = std::min(count, available);

        if (toWrite == 0) return 0;

        size_t firstChunk = std::min(toWrite, m_capacity - head);
        std::copy_n(data, firstChunk, m_buffer.data() + head);

        size_t secondChunk = toWrite - firstChunk;
        if (secondChunk > 0) {
            std::copy_n(data + firstChunk, secondChunk, m_buffer.data());
        }

        m_head.store((head + toWrite) % m_capacity, std::memory_order_release);
        return toWrite;
    }

    size_t read(T* destination, size_t count) {
        size_t head = m_head.load(std::memory_order_acquire);
        size_t tail = m_tail.load(std::memory_order_relaxed);

        size_t available = (head >= tail) ? (head - tail) : (m_capacity - tail + head);
        size_t toRead = std::min(count, available);

        if (toRead == 0) return 0;

        size_t firstChunk = std::min(toRead, m_capacity - tail);
        std::copy_n(m_buffer.data() + tail, firstChunk, destination);

        size_t secondChunk = toRead - firstChunk;
        if (secondChunk > 0) {
            std::copy_n(m_buffer.data(), secondChunk, destination + firstChunk);
        }

        m_tail.store((tail + toRead) % m_capacity, std::memory_order_release);
        return toRead;
    }

    [[nodiscard]] size_t size() const {
        size_t head = m_head.load(std::memory_order_acquire);
        size_t tail = m_tail.load(std::memory_order_acquire);
        return (head >= tail) ? (head - tail) : (m_capacity - tail + head);
    }

    [[nodiscard]] size_t capacity() const {
        return m_capacity - 1;
    }

    void clear() {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_release);
    }

private:
    const size_t m_capacity;
    std::vector<T> m_buffer;
    alignas(64) std::atomic<size_t> m_head;
    alignas(64) std::atomic<size_t> m_tail;
};

} // namespace RadiosondePI::DSP
