#include "async_writer.hpp"

AsyncWriter::AsyncWriter(std::string path, WriteFn write) : path_(std::move(path)), write_(std::move(write)) {
    try {
        thread_ = std::thread([this] { loop(); });
    } catch (...) {
        sync_ = true;  // no thread available: submit() writes on the caller's thread
    }
}

AsyncWriter::~AsyncWriter() {
    {
        std::lock_guard<std::mutex> lock(m_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void AsyncWriter::submit(std::vector<uint8_t> data) {
    if (sync_) {
        bool ok = false;
        try {
            ok = write_(path_, data);
        } catch (...) {
        }
        std::lock_guard<std::mutex> lock(m_);
        if (!ok) failed_ = true;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_);
        pending_ = std::move(data);
    }
    cv_.notify_all();
}

void AsyncWriter::waitIdle() {
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [this] { return !pending_ && !busy_; });
}

bool AsyncWriter::takeFailure() {
    std::lock_guard<std::mutex> lock(m_);
    const bool f = failed_;
    failed_ = false;
    return f;
}

void AsyncWriter::loop() {
    std::unique_lock<std::mutex> lock(m_);
    for (;;) {
        cv_.wait(lock, [this] { return pending_ || stop_; });
        if (!pending_) return;  // stop requested and nothing left to write
        std::vector<uint8_t> data = std::move(*pending_);
        pending_.reset();
        busy_ = true;
        lock.unlock();
        bool ok = false;
        try {
            ok = write_(path_, data);
        } catch (...) {
        }
        lock.lock();
        busy_ = false;
        if (!ok) failed_ = true;
        cv_.notify_all();
    }
}
