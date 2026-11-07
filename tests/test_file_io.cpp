#include "file_io.hpp"

#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
fs::path dir() {
    static fs::path d = [] {
        auto p = fs::temp_directory_path() / "eqx_test_fileio";
        fs::remove_all(p);
        fs::create_directories(p);
        return p;
    }();
    return d;
}
std::string u8(const fs::path& p) {
    auto s = p.u8string();
    return std::string(s.begin(), s.end());
}
}  // namespace

TEST_CASE("atomic write replaces content") {
    auto p = u8(dir() / "a.srm");
    REQUIRE(writeFileAtomic(p, std::vector<uint8_t>{1, 2, 3}));
    REQUIRE(writeFileAtomic(p, std::vector<uint8_t>{9, 8}));
    CHECK(readFile(p) == std::vector<uint8_t>{9, 8});
    CHECK_FALSE(fs::exists(p + ".tmp"));
}

TEST_CASE("creates missing directory") {
    auto p = u8(dir() / "x" / "y" / "b.srm");
    REQUIRE(writeFileAtomic(p, std::vector<uint8_t>{5}));
    CHECK(readFile(p) == std::vector<uint8_t>{5});
}

// Portable failures: (1) a parent path component is a regular file, so the folder
// cannot be created; (2) the target is an existing directory, so the rename cannot
// replace it. In both, the pre-existing entry is left as it was and no .tmp remains.
TEST_CASE("failed write keeps old file") {
    auto blocker = dir() / "blocker";
    { std::ofstream f(blocker, std::ios::binary); f << "old"; }
    CHECK_FALSE(writeFileAtomic(u8(blocker / "sub" / "c.srm"), std::vector<uint8_t>{1}));
    CHECK(readFile(u8(blocker)) == std::vector<uint8_t>{'o', 'l', 'd'});

    auto target = dir() / "isdir";
    fs::create_directories(target);
    { std::ofstream f(target / "keep", std::ios::binary); f << "k"; }
    CHECK_FALSE(writeFileAtomic(u8(target), std::vector<uint8_t>{1}));
    CHECK(fs::is_directory(target));
    CHECK(fs::exists(target / "keep"));
    CHECK_FALSE(fs::exists(u8(target) + ".tmp"));
}

// Portable: <target>.tmp is a directory, so the temp file cannot be opened; the write
// fails and the existing target keeps its old bytes.
TEST_CASE("failed write leaves existing target content") {
    auto t = dir() / "keepme.srm";
    REQUIRE(writeFileAtomic(u8(t), std::vector<uint8_t>{1, 2, 3}));
    fs::create_directories(u8(t) + ".tmp");
    CHECK_FALSE(writeFileAtomic(u8(t), std::vector<uint8_t>{9, 9}));
    CHECK(readFile(u8(t)) == std::vector<uint8_t>{1, 2, 3});
}

TEST_CASE("recovery read falls back to bak then tmp") {
    auto t = u8(dir() / "rec.srm");
    CHECK_FALSE(readFileWithRecovery(t).has_value());
    { std::ofstream f(t + ".tmp", std::ios::binary); f << "T"; }
    CHECK(readFileWithRecovery(t) == std::vector<uint8_t>{'T'});
    { std::ofstream f(t + ".bak", std::ios::binary); f << "B"; }
    CHECK(readFileWithRecovery(t) == std::vector<uint8_t>{'B'});
    { std::ofstream f(t, std::ios::binary); f << "X"; }
    CHECK(readFileWithRecovery(t) == std::vector<uint8_t>{'X'});
}

TEST_CASE("recovery prefers a .bak of the expected size over a short or empty file") {
    auto t = u8(dir() / "size.srm");
    { std::ofstream f(t + ".bak", std::ios::binary); f << "GOOD"; }
    { std::ofstream f(t, std::ios::binary); f << "GO"; }  // truncated by a power cut
    CHECK(readFileWithRecovery(t, 4) == std::vector<uint8_t>{'G', 'O', 'O', 'D'});
    { std::ofstream f(t, std::ios::binary); }  // empty
    CHECK(readFileWithRecovery(t, 4) == std::vector<uint8_t>{'G', 'O', 'O', 'D'});
    CHECK(readFileWithRecovery(t) == std::vector<uint8_t>{'G', 'O', 'O', 'D'});  // empty never wins
}

TEST_CASE("recovery takes a .tmp of the expected size when main and .bak are short") {
    auto t = u8(dir() / "tmpwins.srm");
    { std::ofstream f(t, std::ios::binary); f << "A"; }
    { std::ofstream f(t + ".bak", std::ios::binary); f << "BB"; }
    { std::ofstream f(t + ".tmp", std::ios::binary); f << "CCC"; }
    CHECK(readFileWithRecovery(t, 3) == std::vector<uint8_t>{'C', 'C', 'C'});
}

TEST_CASE("recovery still loads a save of another size when nothing matches") {
    auto t = u8(dir() / "other.srm");
    { std::ofstream f(t, std::ios::binary); f << "12345678"; }  // e.g. imported, padded
    CHECK(readFileWithRecovery(t, 4) == std::vector<uint8_t>{'1', '2', '3', '4', '5', '6', '7', '8'});
}

TEST_CASE("atomic write of large data round-trips") {
    std::vector<uint8_t> big(300 * 1024);
    for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(i * 31);
    auto p = u8(dir() / "big.bin");
    REQUIRE(writeFileAtomic(p, big));
    CHECK(readFile(p) == big);
    CHECK(fileSize(p) == big.size());
    CHECK_FALSE(fileSize(u8(dir() / "missing.bin")).has_value());
    CHECK_FALSE(fileSize(u8(dir())).has_value());  // a folder is not a file
}
