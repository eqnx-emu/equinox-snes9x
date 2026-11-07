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
