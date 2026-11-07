#include "audio_ring.hpp"

#include <algorithm>
#include <cstring>

AudioRing::AudioRing(size_t frames) : buf_(frames * 2, 0), cap_(frames) {}

size_t AudioRing::available() const {
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
}

size_t AudioRing::write(const int16_t* lr, size_t frames) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t tail = tail_.load(std::memory_order_acquire);
    const size_t n = std::min(frames, cap_ - (head - tail));
    for (size_t i = 0; i < n; ++i) {
        const size_t slot = (head + i) % cap_;
        buf_[slot * 2] = lr[i * 2];
        buf_[slot * 2 + 1] = lr[i * 2 + 1];
    }
    head_.store(head + n, std::memory_order_release);
    return n;
}

size_t AudioRing::read(int16_t* lr, size_t frames) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);
    const size_t n = std::min(frames, head - tail);
    for (size_t i = 0; i < n; ++i) {
        const size_t slot = (tail + i) % cap_;
        lr[i * 2] = buf_[slot * 2];
        lr[i * 2 + 1] = buf_[slot * 2 + 1];
    }
    std::memset(lr + n * 2, 0, (frames - n) * 2 * sizeof(int16_t));
    tail_.store(tail + n, std::memory_order_release);
    return n;
}
