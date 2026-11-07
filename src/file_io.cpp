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
            // Only fall back for regular files, never delete a directory.
            std::error_code ec2;
            if (fs::is_regular_file(target, ec2)) {
                fs::remove(target, ec2);
                ec.clear();
                fs::rename(tmp, target, ec);
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
