#include "video_prefs.hpp"

#include "file_io.hpp"

#include <cctype>
#include <filesystem>

namespace {

struct Cursor {
    std::string_view s;
    size_t i = 0;

    bool done() const { return i >= s.size(); }
    void skipSpace() {
        while (!done() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }
    bool eat(char c) {
        skipSpace();
        if (done() || s[i] != c) return false;
        ++i;
        return true;
    }
};

bool readString(Cursor& c, std::string& out) {
    if (!c.eat('"')) return false;
    out.clear();
    while (!c.done()) {
        const char ch = c.s[c.i++];
        if (ch == '"') return true;
        if (ch == '\\') {
            if (c.done()) return false;
            const char e = c.s[c.i++];
            if (e != '"' && e != '\\') return false;
            out.push_back(e);
        } else {
            out.push_back(ch);
        }
    }
    return false;
}

// Numbers, true, false and null: consumed and discarded. Nested values are errors.
bool skipScalar(Cursor& c) {
    c.skipSpace();
    const size_t start = c.i;
    while (!c.done()) {
        const char ch = c.s[c.i];
        if (ch == ',' || ch == '}' || std::isspace(static_cast<unsigned char>(ch))) break;
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '+' && ch != '.')
            return false;
        ++c.i;
    }
    return c.i > start;
}

template <typename E>
struct Entry {
    const char* id;
    E value;
};

constexpr Entry<Size> kSizes[] = {{"original", Size::Original}, {"8:7", Size::Aspect87},
    {"4:3", Size::Aspect43}, {"full", Size::Full}};
constexpr Entry<Smooth> kSmooths[] = {
    {"pixel", Smooth::Pixel}, {"sharp", Smooth::Sharp}, {"soft", Smooth::Soft}};
constexpr Entry<Filter> kFilters[] = {
    {"none", Filter::None}, {"scanlines", Filter::Scanlines}, {"ntsc", Filter::Ntsc}};
constexpr Entry<Border> kBorders[] = {{"equinox", Border::Equinox}, {"cover", Border::Cover},
    {"tv", Border::Tv}, {"black", Border::Black}};

template <typename E, size_t N>
std::optional<E> lookup(const Entry<E> (&table)[N], const std::string& id) {
    for (const auto& e : table)
        if (id == e.id) return e.value;
    return std::nullopt;
}

template <typename E, size_t N>
const char* idOf(const Entry<E> (&table)[N], E value) {
    for (const auto& e : table)
        if (e.value == value) return e.id;
    return table[0].id;
}

template <typename T>
void diff(std::optional<T>& out, T global, T effective) {
    if (effective != global) out = effective;
}

// Sets the named key; a value that is not a known string leaves it absent.
void setKey(VideoPrefs& prefs, const std::string& key, const std::string* value) {
    auto id = [&](auto&& table) {
        return value ? lookup(table, *value) : std::nullopt;
    };
    if (key == "size") prefs.size = id(kSizes);
    else if (key == "smooth") prefs.smooth = id(kSmooths);
    else if (key == "filter") {
        prefs.filter = id(kFilters);
        if (!kNtscEnabled && prefs.filter == Filter::Ntsc) prefs.filter.reset();
    } else if (key == "border") prefs.border = id(kBorders);
}

}  // namespace

VideoPrefs parsePrefs(std::string_view json) {
    if (json.substr(0, 3) == "\xEF\xBB\xBF") json.remove_prefix(3);
    Cursor c{json};
    VideoPrefs prefs;
    if (!c.eat('{')) return {};
    if (!c.eat('}')) {
        for (;;) {
            std::string key;
            if (!readString(c, key) || !c.eat(':')) return {};
            c.skipSpace();
            if (!c.done() && c.s[c.i] == '"') {
                std::string value;
                if (!readString(c, value)) return {};
                setKey(prefs, key, &value);
            } else {
                if (!skipScalar(c)) return {};
                setKey(prefs, key, nullptr);
            }
            if (c.eat(',')) continue;
            if (c.eat('}')) break;
            return {};
        }
    }
    c.skipSpace();
    if (!c.done()) return {};
    return prefs;
}

std::string serializePrefs(const VideoPrefs& prefs) {
    std::string out = "{";
    auto add = [&](const char* key, const char* id) {
        if (out.size() > 1) out += ',';
        out += '"';
        out += key;
        out += "\":\"";
        out += id;
        out += '"';
    };
    if (prefs.size) add("size", toId(*prefs.size));
    if (prefs.smooth) add("smooth", toId(*prefs.smooth));
    if (prefs.filter) add("filter", toId(*prefs.filter));
    if (prefs.border) add("border", toId(*prefs.border));
    out += '}';
    return out;
}

VideoSettings resolve(const VideoPrefs& global, const VideoPrefs& game) {
    VideoSettings s;
    s.size = game.size.value_or(global.size.value_or(s.size));
    s.smooth = game.smooth.value_or(global.smooth.value_or(s.smooth));
    s.filter = game.filter.value_or(global.filter.value_or(s.filter));
    s.border = game.border.value_or(global.border.value_or(s.border));
    return s;
}

VideoPrefs overridesFor(const VideoSettings& global, const VideoSettings& effective) {
    VideoPrefs o;
    diff(o.size, global.size, effective.size);
    diff(o.smooth, global.smooth, effective.smooth);
    diff(o.filter, global.filter, effective.filter);
    diff(o.border, global.border, effective.border);
    return o;
}

Border effectiveBorder(const VideoSettings& settings, bool hasCover) {
    if (settings.border == Border::Cover && !hasCover) return Border::Equinox;
    return settings.border;
}

const char* toId(Size v) { return idOf(kSizes, v); }
const char* toId(Smooth v) { return idOf(kSmooths, v); }
const char* toId(Filter v) { return idOf(kFilters, v); }
const char* toId(Border v) { return idOf(kBorders, v); }

VideoPrefs loadPrefs(const std::string& path) {
    const auto bytes = readFile(path);
    if (!bytes) return {};
    return parsePrefs(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

bool savePrefs(const std::string& path, const VideoPrefs& prefs) {
    namespace fs = std::filesystem;
    if (prefs.empty()) {
        std::error_code ec;
        const fs::path p(std::u8string(path.begin(), path.end()));
        fs::remove(p, ec);
        return !ec;
    }
    const std::string json = serializePrefs(prefs);
    return writeFileAtomic(path, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()));
}
