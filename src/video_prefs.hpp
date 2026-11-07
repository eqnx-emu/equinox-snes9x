#pragma once

#include <optional>
#include <string>
#include <string_view>

enum class Size { Original, Aspect87, Aspect43, Full };
enum class Smooth { Pixel, Sharp, Soft };
enum class Filter { None, Scanlines, Ntsc };
enum class Border { Equinox, Cover, Tv, Black };

// Flipped off when the NTSC filter fails the on-device 60 fps gate.
inline constexpr bool kNtscEnabled = true;

// What a JSON file says. An absent key follows the default beneath it.
struct VideoPrefs {
    std::optional<Size> size;
    std::optional<Smooth> smooth;
    std::optional<Filter> filter;
    std::optional<Border> border;

    bool empty() const { return !size && !smooth && !filter && !border; }
};

// Everything resolved; the initializers are the defaults.
struct VideoSettings {
    Size size = Size::Original;
    Smooth smooth = Smooth::Pixel;
    Filter filter = Filter::None;
    Border border = Border::Equinox;

    bool operator==(const VideoSettings&) const = default;
};

// A JSON object. Unknown keys and values of any JSON type are ignored, so files
// from newer versions still load; any syntax error yields empty prefs.
VideoPrefs parsePrefs(std::string_view json);
std::string serializePrefs(const VideoPrefs& prefs);

// A missing or unreadable file is empty prefs.
VideoPrefs loadPrefs(const std::string& path);
// Empty prefs delete the file. Returns false on failure; never throws.
bool savePrefs(const std::string& path, const VideoPrefs& prefs);

// Game over global over the spec defaults.
VideoSettings resolve(const VideoPrefs& global, const VideoPrefs& game);
// Only the keys of `effective` that differ from `global`.
VideoPrefs overridesFor(const VideoSettings& global, const VideoSettings& effective);

// A cover border without a cover falls back to the Equinox frame.
Border effectiveBorder(const VideoSettings& settings, bool hasCover);

const char* toId(Size);
const char* toId(Smooth);
const char* toId(Filter);
const char* toId(Border);
