#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

enum class RomError { None, NotFound, Unreadable, NoRomInZip, Unsupported, TooBig };

struct Rom {
    std::vector<uint8_t> data;
    std::string innerName;  // entry name when loaded from a zip, empty otherwise
};

// .sfc/.smc are read whole; for .zip the first .sfc/.smc entry in central-directory order.
// Limits, checked before reading: 8 MiB for the ROM (the largest SNES ROM is about 6 MiB)
// and 16 MiB for a zip; bigger gives TooBig. The 512-byte copier header is left
// for Snes9x to handle.
std::variant<Rom, RomError> loadRom(const std::string& path);
