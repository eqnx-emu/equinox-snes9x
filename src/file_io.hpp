#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Reads a whole file; paths are UTF-8. Returns nullopt if it cannot be opened or read.
std::optional<std::vector<uint8_t>> readFile(const std::string& path);
