#include "file_io.hpp"
#include "rom.hpp"

#include "doctest/doctest.h"
#include "miniz/miniz.h"
#include "sevenz_fixtures.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path tempDir() {
    static fs::path d = [] {
        auto p = fs::temp_directory_path() / "eqx_test_rom";
        fs::remove_all(p);
        fs::create_directories(p);
        return p;
    }();
    return d;
}

std::string writeBytes(const std::string& name, const std::vector<uint8_t>& bytes) {
    auto p = tempDir() / fs::path(std::u8string(name.begin(), name.end()));
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string writeZip(const std::string& name, const std::vector<std::pair<std::string, std::string>>& entries) {
    mz_zip_archive z{};
    REQUIRE(mz_zip_writer_init_heap(&z, 0, 0));
    for (auto& [n, d] : entries)
        REQUIRE(mz_zip_writer_add_mem(&z, n.c_str(), d.data(), d.size(), MZ_DEFAULT_LEVEL));
    void* buf = nullptr;
    size_t sz = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&z, &buf, &sz));
    std::vector<uint8_t> bytes(static_cast<uint8_t*>(buf), static_cast<uint8_t*>(buf) + sz);
    mz_zip_writer_end(&z);
    mz_free(buf);
    return writeBytes(name, bytes);
}

}  // namespace

TEST_CASE("loads plain sfc") {
    auto path = writeBytes("plain.sfc", std::vector<uint8_t>(0x80000, 0x42));
    auto r = loadRom(path);
    REQUIRE(std::holds_alternative<Rom>(r));
    CHECK(std::get<Rom>(r).data.size() == 0x80000);
}

TEST_CASE("loads first sfc inside zip") {
    auto path = writeZip("pack.zip", {{"readme.txt", "hi"}, {"Game.sfc", std::string(1024, 'x')}});
    auto r = loadRom(path);
    REQUIRE(std::holds_alternative<Rom>(r));
    CHECK(std::get<Rom>(r).innerName == "Game.sfc");
    CHECK(std::get<Rom>(r).data.size() == 1024);
}

TEST_CASE("zip without rom") {
    auto path = writeZip("norom.zip", {{"readme.txt", "hi"}});
    auto r = loadRom(path);
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::NoRomInZip);
}

TEST_CASE("missing file") {
    auto r = loadRom((tempDir() / "nope.sfc").string());
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::NotFound);
}

TEST_CASE("unsupported extension") {
    auto path = writeBytes("game.nes", std::vector<uint8_t>(16, 1));
    auto r = loadRom(path);
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::Unsupported);
}

TEST_CASE("uppercase extension") {
    auto path = writeBytes("GAME.SFC", std::vector<uint8_t>(0x8000, 7));
    CHECK(std::holds_alternative<Rom>(loadRom(path)));
}

TEST_CASE("smc and utf-8 path with spaces") {
    auto path = writeBytes("Pokémon World (USA).smc", std::vector<uint8_t>(0x200 + 0x8000, 3));
    auto r = loadRom(path);
    REQUIRE(std::holds_alternative<Rom>(r));
    CHECK(std::get<Rom>(r).data.size() == 0x200 + 0x8000);
}

TEST_CASE("too big plain file") {
    auto path = writeBytes("huge.sfc", std::vector<uint8_t>(8 * 1024 * 1024 + 1, 0));
    auto r = loadRom(path);
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::TooBig);
}

TEST_CASE("readFile") {
    auto path = writeBytes("raw.bin", {1, 2, 3});
    auto d = readFile(path);
    REQUIRE(d);
    CHECK(d->size() == 3);
    CHECK_FALSE(readFile((tempDir() / "absent").string()));
}

// A file of `size` zero bytes, made by extending an empty file (no big buffer in the test).
static std::string sizedFile(const std::string& name, uintmax_t size) {
    auto path = writeBytes(name, {});
    fs::resize_file(fs::path(std::u8string(path.begin(), path.end())), size);
    return path;
}

TEST_CASE("zip bigger than 16 MiB is refused before reading") {
    auto r = loadRom(sizedFile("romset.zip", 16u * 1024 * 1024 + 1));
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::TooBig);
}

TEST_CASE("zip at the 16 MiB cap is read, and garbage is unreadable") {
    auto r = loadRom(sizedFile("cap.zip", 16u * 1024 * 1024));
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::Unreadable);
}

TEST_CASE("corrupt zip is unreadable") {
    auto r = loadRom(writeBytes("corrupt.zip", {'P', 'K', 3, 4, 0, 0, 1, 2, 3}));
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::Unreadable);
}

TEST_CASE("plain rom exactly 8 MiB loads") {
    auto r = loadRom(sizedFile("max.sfc", 8u * 1024 * 1024));
    REQUIRE(std::holds_alternative<Rom>(r));
    CHECK(std::get<Rom>(r).data.size() == 8u * 1024 * 1024);
}

TEST_CASE("huge plain rom is refused without reading it") {
    auto r = loadRom(sizedFile("romset.smc", 64ull * 1024 * 1024));
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::TooBig);
}

TEST_CASE("loads the first rom inside a 7z") {
    auto r = loadRom(writeBytes("pack.7z", kSevenZipRom));
    REQUIRE(std::holds_alternative<Rom>(r));
    CHECK(std::get<Rom>(r).data == romFixture());
    CHECK(std::get<Rom>(r).innerName == "Game.sfc");
}

TEST_CASE("7z without a rom is reported as such") {
    auto r = loadRom(writeBytes("norom.7z", kSevenZipNoRom));
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::NoRomInZip);
}

TEST_CASE("damaged 7z is unreadable") {
    auto r = loadRom(writeBytes("bad.7z", std::vector<uint8_t>(64, 0x37)));
    REQUIRE(std::holds_alternative<RomError>(r));
    CHECK(std::get<RomError>(r) == RomError::Unreadable);
}

TEST_CASE("cleanup temp dir") {
    std::error_code ec;
    fs::remove_all(tempDir(), ec);
}
