#include "rom.hpp"

#include "file_io.hpp"
#include "miniz/miniz.h"
#include "sevenz.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <new>

namespace {

constexpr size_t kMaxRomBytes = 8u * 1024 * 1024;
// One compressed SNES ROM is at most ~6 MiB; anything bigger is a romset or not a ROM.
constexpr uint64_t kMaxZipBytes = 16u * 1024 * 1024;
// A solid .7z must be unpacked up to the ROM, so this also bounds what a pack can ask for.
constexpr uint64_t kMax7zBlockBytes = 32u * 1024 * 1024;

std::string extensionOf(const std::string& name) {
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = name.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext;
}

bool isRomExt(const std::string& ext) { return ext == ".sfc" || ext == ".smc"; }

std::variant<Rom, RomError> loadFromZip(const std::vector<uint8_t>& zipData) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zipData.data(), zipData.size(), 0)) return RomError::Unreadable;

    std::variant<Rom, RomError> result = RomError::NoRomInZip;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory) continue;
        std::string name = st.m_filename;
        if (!isRomExt(extensionOf(name))) continue;
        if (st.m_uncomp_size > kMaxRomBytes) {
            result = RomError::TooBig;
            break;
        }
        Rom rom;
        rom.innerName = name;
        try {
            rom.data.resize(static_cast<size_t>(st.m_uncomp_size));
        } catch (const std::bad_alloc&) {
            result = RomError::TooBig;
            break;
        }
        if (!mz_zip_reader_extract_to_mem(&zip, i, rom.data.data(), rom.data.size(), 0)) {
            result = RomError::Unreadable;
            break;
        }
        result = std::move(rom);
        break;
    }
    mz_zip_reader_end(&zip);
    return result;
}

std::variant<Rom, RomError> loadFrom7z(const std::vector<uint8_t>& archive) {
    sevenz::Found found;
    const auto isRom = [](const std::string& name) { return isRomExt(extensionOf(name)); };
    switch (sevenz::firstMatching(archive.data(), archive.size(), isRom, kMaxRomBytes, kMax7zBlockBytes,
                                  true, found)) {
    case sevenz::Status::Ok:
        return Rom{std::move(found.bytes), std::move(found.name)};
    case sevenz::Status::NoMatch:
        return RomError::NoRomInZip;
    case sevenz::Status::TooBig:
        return RomError::TooBig;
    case sevenz::Status::Damaged:
        break;
    }
    return RomError::Unreadable;
}

}  // namespace

std::variant<Rom, RomError> loadRom(const std::string& path) {
    const std::string ext = extensionOf(path);
    const bool sevenZip = ext == ".7z";
    const bool zip = ext == ".zip" || sevenZip;
    if (!zip && !isRomExt(ext)) return RomError::Unsupported;

    // Check the size before reading, so a romset zip of hundreds of MB is refused at once.
    const auto size = fileSize(path);
    if (!size) {
        // Distinguish a missing file from one that exists but can't be read.
        std::error_code ec;
        const bool exists = std::filesystem::exists(std::filesystem::path(std::u8string(path.begin(), path.end())), ec);
        return exists ? RomError::Unreadable : RomError::NotFound;
    }
    if (*size > (zip ? kMaxZipBytes : kMaxRomBytes)) return RomError::TooBig;

    auto bytes = readFile(path);
    if (!bytes) return RomError::Unreadable;
    if (!zip) {
        if (bytes->size() > kMaxRomBytes) return RomError::TooBig;  // grew since the size check
        return Rom{std::move(*bytes), {}};
    }
    if (bytes->size() > kMaxZipBytes) return RomError::TooBig;
    return sevenZip ? loadFrom7z(*bytes) : loadFromZip(*bytes);
}
