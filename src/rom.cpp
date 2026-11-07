#include "rom.hpp"

#include "file_io.hpp"
#include "miniz/miniz.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {

constexpr size_t kMaxRomBytes = 8u * 1024 * 1024;

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
        rom.data.resize(static_cast<size_t>(st.m_uncomp_size));
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

}  // namespace

std::variant<Rom, RomError> loadRom(const std::string& path) {
    const std::string ext = extensionOf(path);
    const bool zip = ext == ".zip";
    if (!zip && !isRomExt(ext)) return RomError::Unsupported;

    auto bytes = readFile(path);
    if (!bytes) {
        // Distinguish a missing file from one that exists but can't be read.
        std::error_code ec;
        const bool exists = std::filesystem::exists(std::filesystem::path(std::u8string(path.begin(), path.end())), ec);
        return exists ? RomError::Unreadable : RomError::NotFound;
    }
    if (!zip) {
        if (bytes->size() > kMaxRomBytes) return RomError::TooBig;
        return Rom{std::move(*bytes), {}};
    }
    return loadFromZip(*bytes);
}
