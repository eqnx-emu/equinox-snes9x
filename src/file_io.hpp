#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Reads a whole file; paths are UTF-8. Returns nullopt if it cannot be opened or read.
std::optional<std::vector<uint8_t>> readFile(const std::string& path);

// Size of a regular file in bytes, or nullopt if it is missing or not a regular file.
std::optional<uint64_t> fileSize(const std::string& path);

// Writes <path>.tmp, flushes it to storage (fsync), then renames it over path; creates
// the parent folder if missing. Returns false on any failure (never throws); the old
// file is then left intact. A .tmp is removed on failure, except when the FAT fallback
// could not restore the old file: then .tmp and .bak are kept for readFileWithRecovery.
bool writeFileAtomic(const std::string& path, std::span<const uint8_t> data);

// Reads path, else <path>.bak, else <path>.tmp: recovers from an interrupted
// writeFileAtomic on filesystems where the rename needs a remove-then-rename.
// A candidate is valid when its size equals expectedSize (if given), else when it is
// not empty. If none is valid but expectedSize was given, the first non-empty
// candidate is returned, so a save of another size is still loaded rather than lost.
std::optional<std::vector<uint8_t>> readFileWithRecovery(const std::string& path,
                                                         std::optional<size_t> expectedSize = {});
