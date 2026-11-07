#include "file_io.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

std::optional<std::vector<uint8_t>> readFile(const std::string& path) {
    try {
        std::filesystem::path p(std::u8string(path.begin(), path.end()));
        std::ifstream f(p, std::ios::binary);
        if (!f) return std::nullopt;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (f.bad()) return std::nullopt;
        return data;
    } catch (...) {
        return std::nullopt;
    }
}

bool writeFileAtomic(const std::string& path, std::span<const uint8_t> data) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path target;
    fs::path tmp;
    try {
        target = fs::path(std::u8string(path.begin(), path.end()));
        tmp = target;
        tmp += ".tmp";
        if (target.has_parent_path()) {
            fs::create_directories(target.parent_path(), ec);
            if (ec) return false;
        }
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) return false;
            f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            f.flush();
            if (!f) {
                f.close();
                fs::remove(tmp, ec);
                return false;
            }
        }
        fs::rename(tmp, target, ec);
        if (ec) {
            // Some filesystems (Switch FAT) refuse to rename over an existing file.
            // Only fall back for regular files. The old file is moved to <path>.bak,
            // never deleted, until the new one is in place; on failure it is restored.
            std::error_code ec2;
            if (fs::is_regular_file(target, ec2)) {
                fs::path bak = target;
                bak += ".bak";
                fs::remove(bak, ec2);
                fs::rename(target, bak, ec2);
                if (ec2) {
                    fs::remove(tmp, ec2);  // target untouched
                    return false;
                }
                ec.clear();
                fs::rename(tmp, target, ec);
                if (ec) {
                    std::error_code ec3;
                    fs::rename(bak, target, ec3);
                    if (!ec3) fs::remove(tmp, ec3);  // restored; otherwise keep .tmp and .bak for recovery
                    return false;
                }
                fs::remove(bak, ec2);
                return true;
            }
        }
        if (ec) {
            fs::remove(tmp, ec);
            return false;
        }
        return true;
    } catch (...) {
        try {
            std::error_code e2;
            if (!tmp.empty()) fs::remove(tmp, e2);
        } catch (...) {
        }
        return false;
    }
}

std::optional<std::vector<uint8_t>> readFileWithRecovery(const std::string& path) {
    for (const char* suffix : {"", ".bak", ".tmp"})
        if (auto d = readFile(path + suffix)) return d;
    return std::nullopt;
}
