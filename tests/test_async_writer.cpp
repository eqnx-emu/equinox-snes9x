#include "async_writer.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "doctest/doctest.h"

namespace {
// A write function that records every call and can be held closed by the test.
struct FakeDisk {
    std::mutex m;
    std::condition_variable cv;
    bool open = true;
    std::atomic<int> started{0};
    std::vector<std::vector<uint8_t>> writes;
    bool result = true;

    AsyncWriter::WriteFn fn() {
        return [this](const std::string&, std::span<const uint8_t> d) {
            ++started;
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, [this] { return open; });
            writes.emplace_back(d.begin(), d.end());
            return result;
        };
    }
    void hold() {
        std::lock_guard<std::mutex> lock(m);
        open = false;
    }
    void release() {
        {
            std::lock_guard<std::mutex> lock(m);
            open = true;
        }
        cv.notify_all();
    }
};
}  // namespace

TEST_CASE("async writer writes a submitted buffer and waitIdle waits for it") {
    FakeDisk disk;
    AsyncWriter w("x.srm", disk.fn());
    w.submit({1, 2, 3});
    w.waitIdle();
    REQUIRE(disk.writes.size() == 1);
    CHECK(disk.writes[0] == std::vector<uint8_t>{1, 2, 3});
    CHECK_FALSE(w.takeFailure());
}

TEST_CASE("async writer coalesces: the latest buffer wins") {
    FakeDisk disk;
    AsyncWriter w("x.srm", disk.fn());
    disk.hold();
    w.submit({1});
    while (disk.started.load() == 0) std::this_thread::yield();  // {1} is in flight
    w.submit({2});
    w.submit({3});  // replaces {2} before it is written
    disk.release();
    w.waitIdle();
    REQUIRE(disk.writes.size() == 2);
    CHECK(disk.writes[0] == std::vector<uint8_t>{1});
    CHECK(disk.writes[1] == std::vector<uint8_t>{3});
}

TEST_CASE("async writer reports a failure once") {
    FakeDisk disk;
    disk.result = false;
    AsyncWriter w("x.srm", disk.fn());
    w.submit({7});
    w.waitIdle();
    CHECK(w.takeFailure());
    CHECK_FALSE(w.takeFailure());
}

TEST_CASE("async writer destructor finishes the pending write") {
    FakeDisk disk;
    std::thread releaser;
    {
        AsyncWriter w("x.srm", disk.fn());
        disk.hold();
        w.submit({1});
        while (disk.started.load() == 0) std::this_thread::yield();
        w.submit({9});
        releaser = std::thread([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            disk.release();
        });
    }
    releaser.join();
    REQUIRE(disk.writes.size() == 2);
    CHECK(disk.writes[1] == std::vector<uint8_t>{9});
}
