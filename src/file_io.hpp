#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Reads a whole file; paths are UTF-8. Returns nullopt if it cannot be opened or read.
std::optional<std::vector<uint8_t>> readFile(const std::string& path);

// Writes <path>.tmp, then renames it over path; creates the parent folder if missing.
// Returns false on any failure (never throws); the old file is then left intact and
// a leftover .tmp is removed.
bool writeFileAtomic(const std::string& path, std::span<const uint8_t> data);

// readFile(path), else <path>.bak, else <path>.tmp: recovers from an interrupted
// writeFileAtomic on filesystems where the rename needs a remove-then-rename.
std::optional<std::vector<uint8_t>> readFileWithRecovery(const std::string& path);
