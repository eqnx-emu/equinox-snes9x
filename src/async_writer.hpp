#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

// Writes one file from a worker thread, so a slow SD write never stalls the frame loop.
// Latest wins: a buffer submitted while another is waiting replaces it. One write at a
// time; waitIdle() blocks until nothing is pending or in flight. If the thread cannot be
// created, submit() writes synchronously instead.
class AsyncWriter {
public:
    using WriteFn = std::function<bool(const std::string& path, std::span<const uint8_t> data)>;

    AsyncWriter(std::string path, WriteFn write);
    ~AsyncWriter();  // finishes the pending write, then joins
    AsyncWriter(const AsyncWriter&) = delete;
    AsyncWriter& operator=(const AsyncWriter&) = delete;

    void submit(std::vector<uint8_t> data);
    void waitIdle();
    // True once for each run of failures since the last call (polled on the main thread).
    bool takeFailure();

private:
    void loop();

    const std::string path_;
    const WriteFn write_;
    std::mutex m_;
    std::condition_variable cv_;
    std::optional<std::vector<uint8_t>> pending_;
    bool busy_ = false, stop_ = false, failed_ = false, sync_ = false;
    std::thread thread_;
};
