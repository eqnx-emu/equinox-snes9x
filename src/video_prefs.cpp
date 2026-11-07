#include "video_prefs.hpp"

#include "file_io.hpp"

#include <cctype>
#include <cstdint>
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

bool readHex4(Cursor& c, uint32_t& out) {
    if (c.s.size() - c.i < 4) return false;
    out = 0;
    for (int k = 0; k < 4; ++k) {
        const char h = c.s[c.i++];
        out <<= 4;
        if (h >= '0' && h <= '9') out |= h - '0';
        else if (h >= 'a' && h <= 'f') out |= h - 'a' + 10;
        else if (h >= 'A' && h <= 'F') out |= h - 'A' + 10;
        else return false;
    }
    return true;
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// A JSON string with every escape decoded to UTF-8.
bool readString(Cursor& c, std::string& out) {
    if (!c.eat('"')) return false;
    out.clear();
    while (!c.done()) {
        const char ch = c.s[c.i++];
        if (ch == '"') return true;
        if (static_cast<unsigned char>(ch) < 0x20) return false;
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (c.done()) return false;
        switch (c.s[c.i++]) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                uint32_t cp;
                if (!readHex4(c, cp)) return false;
                // A high surrogate followed by an escaped low one is one code point.
                if (cp >= 0xD800 && cp < 0xDC00 && c.s.substr(c.i, 2) == "\\u") {
                    Cursor peek = c;
                    peek.i += 2;
                    uint32_t lo;
                    if (readHex4(peek, lo) && lo >= 0xDC00 && lo < 0xE000) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        c.i = peek.i;
                    }
                }
                appendUtf8(out, cp);
                break;
            }
            default: return false;
        }
    }
    return false;
}

bool skipLiteral(Cursor& c, std::string_view word) {
    if (c.s.substr(c.i, word.size()) != word) return false;
    c.i += word.size();
    return true;
}

bool skipDigits(Cursor& c) {
    const size_t start = c.i;
    while (!c.done() && std::isdigit(static_cast<unsigned char>(c.s[c.i]))) ++c.i;
    return c.i > start;
}

bool skipNumber(Cursor& c) {
    if (!c.done() && c.s[c.i] == '-') ++c.i;
    if (!c.done() && c.s[c.i] == '0') ++c.i;
    else if (!skipDigits(c)) return false;
    if (!c.done() && c.s[c.i] == '.') {
        ++c.i;
        if (!skipDigits(c)) return false;
    }
    if (!c.done() && (c.s[c.i] == 'e' || c.s[c.i] == 'E')) {
        ++c.i;
        if (!c.done() && (c.s[c.i] == '+' || c.s[c.i] == '-')) ++c.i;
        if (!skipDigits(c)) return false;
    }
    return true;
}

constexpr int kMaxDepth = 64;

// Any JSON value, consumed and discarded.
bool skipValue(Cursor& c, int depth = 0) {
    if (depth > kMaxDepth) return false;
    c.skipSpace();
    if (c.done()) return false;
    std::string ignored;
    switch (c.s[c.i]) {
        case '"': return readString(c, ignored);
        case '{':
            ++c.i;
            if (c.eat('}')) return true;
            do {
                if (!readString(c, ignored) || !c.eat(':') || !skipValue(c, depth + 1)) return false;
            } while (c.eat(','));
            return c.eat('}');
        case '[':
            ++c.i;
            if (c.eat(']')) return true;
            do {
                if (!skipValue(c, depth + 1)) return false;
            } while (c.eat(','));
            return c.eat(']');
        case 't': return skipLiteral(c, "true");
        case 'f': return skipLiteral(c, "false");
        case 'n': return skipLiteral(c, "null");
        default: return skipNumber(c);
    }
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
void update(std::optional<T>& stored, T global, T effective) {
    if (effective == stored.value_or(global)) return;  // untouched: kept as it was
    if (effective == global) stored.reset();
    else stored = effective;
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
                if (!skipValue(c)) return {};
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

VideoPrefs updateOverrides(VideoPrefs stored, const VideoSettings& global,
                           const VideoSettings& effective) {
    update(stored.size, global.size, effective.size);
    update(stored.smooth, global.smooth, effective.smooth);
    update(stored.filter, global.filter, effective.filter);
    update(stored.border, global.border, effective.border);
    return stored;
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
    const auto bytes = readFileWithRecovery(path);  // an interrupted save leaves .bak/.tmp
    if (!bytes) return {};
    return parsePrefs(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

bool savePrefs(const std::string& path, const VideoPrefs& prefs) {
    namespace fs = std::filesystem;
    if (prefs.empty()) {
        std::error_code ec;
        const fs::path p(std::u8string(path.begin(), path.end()));
        fs::remove(p, ec);
        if (ec) return false;
        // Leftovers of an interrupted save would otherwise be read back by loadPrefs.
        for (const char* suffix : {".bak", ".tmp"}) {
            const std::string side = path + suffix;
            fs::remove(fs::path(std::u8string(side.begin(), side.end())), ec);
            if (ec) return false;
        }
        return true;
    }
    const std::string json = serializePrefs(prefs);
    return writeFileAtomic(path, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()));
}
