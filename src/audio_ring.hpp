#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Interleaved stereo int16 ring buffer. One producer thread calls write(), one
// consumer thread calls read(). Indices are free-running counters, so the ring
// holds exactly `frames` frames.
class AudioRing {
public:
    explicit AudioRing(size_t frames);

    // Returns how many frames fit; the rest are dropped (the oldest data is kept).
    size_t write(const int16_t* lr, size_t frames);
    // Returns how many real frames were read; the remainder of `frames` is silence.
    size_t read(int16_t* lr, size_t frames);
    size_t available() const;

private:
    std::vector<int16_t> buf_;
    size_t cap_;
    std::atomic<size_t> head_{0};  // total frames written (producer)
    std::atomic<size_t> tail_{0};  // total frames read (consumer)
};
