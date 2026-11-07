#include "file_io.hpp"

#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

std::filesystem::path toPath(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::FILE* openFile(const std::filesystem::path& p, bool write) {
#ifdef _WIN32
    return _wfopen(p.c_str(), write ? L"wb" : L"rb");
#else
    return std::fopen(p.c_str(), write ? "wb" : "rb");
#endif
}

// Pushes the OS and FS caches to storage. On libnx, fsync maps to fsFileFlush.
bool syncFile(std::FILE* f) {
#ifdef _WIN32
    return _commit(_fileno(f)) == 0;
#else
    return fsync(fileno(f)) == 0;
#endif
}

// fopen/fwrite/fflush/fsync/fclose, every step checked.
bool writeDurable(const std::filesystem::path& p, std::span<const uint8_t> data) {
    std::FILE* f = openFile(p, true);
    if (!f) return false;
    bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = ok && std::fflush(f) == 0;
    ok = ok && syncFile(f);
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

}  // namespace

std::optional<std::vector<uint8_t>> readFile(const std::string& path) {
    try {
        std::FILE* f = openFile(toPath(path), false);
        if (!f) return std::nullopt;
        std::vector<uint8_t> data;
        std::error_code ec;
        const auto size = std::filesystem::file_size(toPath(path), ec);
        if (!ec) data.reserve(static_cast<size_t>(size));
        uint8_t buf[64 * 1024];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + n);
        const bool bad = std::ferror(f) != 0;
        std::fclose(f);
        if (bad) return std::nullopt;
        return data;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<uint64_t> fileSize(const std::string& path) {
    try {
        std::error_code ec;
        const auto p = toPath(path);
        if (!std::filesystem::is_regular_file(p, ec) || ec) return std::nullopt;
        const auto size = std::filesystem::file_size(p, ec);
        if (ec) return std::nullopt;
        return static_cast<uint64_t>(size);
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
        target = toPath(path);
        tmp = target;
        tmp += ".tmp";
        if (target.has_parent_path()) {
            fs::create_directories(target.parent_path(), ec);
            if (ec) return false;
        }
        if (!writeDurable(tmp, data)) {
            fs::remove(tmp, ec);
            return false;
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

std::optional<std::vector<uint8_t>> readFileWithRecovery(const std::string& path,
                                                         std::optional<size_t> expectedSize) {
    std::optional<std::vector<uint8_t>> fallback;
    for (const char* suffix : {"", ".bak", ".tmp"}) {
        auto d = readFile(path + suffix);
        if (!d || d->empty()) continue;
        if (!expectedSize || d->size() == *expectedSize) return d;
        if (!fallback) fallback = std::move(d);
    }
    return fallback;
}
