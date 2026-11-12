#include "menu_text.hpp"

#include <cstdio>

std::string playedFor(Lang lang, unsigned long seconds) {
    const bool pt = lang == Lang::PtBR;
    const std::string lead = pt ? "Jogando há " : "Playing for ";
    const unsigned long minutes = seconds / 60;
    if (minutes == 0) return lead + (pt ? "menos de 1 min" : "less than 1 min");
    char buf[32];
    if (minutes < 60) std::snprintf(buf, sizeof buf, "%lu min", minutes);
    else std::snprintf(buf, sizeof buf, "%lu h %02lu min", minutes / 60, minutes % 60);
    return lead + buf;
}

namespace {
bool isLeap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

bool isYesterday(const std::tm& when, const std::tm& now) {
    if (when.tm_year == now.tm_year) return when.tm_yday + 1 == now.tm_yday;
    const int lastDay = isLeap(when.tm_year + 1900) ? 365 : 364;
    return when.tm_year + 1 == now.tm_year && now.tm_yday == 0 && when.tm_yday == lastDay;
}
}  // namespace

std::string savedAt(Lang lang, const std::tm& when, const std::tm& now) {
    const bool pt = lang == Lang::PtBR;
    char clock[8];
    std::snprintf(clock, sizeof clock, "%02d:%02d", when.tm_hour, when.tm_min);
    const std::string at = pt ? " às " : " at ";
    if (when.tm_year == now.tm_year && when.tm_yday == now.tm_yday) return (pt ? "Hoje" : "Today") + at + clock;
    if (isYesterday(when, now)) return (pt ? "Ontem" : "Yesterday") + at + clock;
    char day[8];
    if (pt) std::snprintf(day, sizeof day, "%02d/%02d", when.tm_mday, when.tm_mon + 1);
    else std::snprintf(day, sizeof day, "%02d/%02d", when.tm_mon + 1, when.tm_mday);
    return day + at + clock;
}

namespace {
// Drops whole UTF-8 characters from the end until s + "..." fits.
std::string ellipsize(std::string s, int maxW, const std::function<int(const std::string&)>& measure) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    while (!s.empty() && measure(s + "...") > maxW) {
        size_t n = s.size() - 1;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
        s.resize(n);
        while (!s.empty() && s.back() == ' ') s.pop_back();
    }
    return s + "...";
}

// The longest prefix of word, on character boundaries, that fits in maxW (at least one character).
size_t fittingPrefix(const std::string& word, int maxW, const std::function<int(const std::string&)>& measure) {
    size_t best = 0;
    for (size_t i = 1; i <= word.size(); ++i) {
        if (i < word.size() && (static_cast<unsigned char>(word[i]) & 0xC0) == 0x80) continue;
        if (measure(word.substr(0, i)) > maxW) break;
        best = i;
    }
    if (best == 0) {
        best = 1;
        while (best < word.size() && (static_cast<unsigned char>(word[best]) & 0xC0) == 0x80) ++best;
    }
    return best;
}
}  // namespace

std::vector<std::string> wrapLines(const std::string& text, int maxW,
                                   const std::function<int(const std::string&)>& measure, int maxLines) {
    std::vector<std::string> words;
    for (size_t i = 0; i < text.size();) {
        while (i < text.size() && text[i] == ' ') ++i;
        size_t j = i;
        while (j < text.size() && text[j] != ' ') ++j;
        if (j > i) words.push_back(text.substr(i, j - i));
        i = j;
    }
    std::vector<std::string> lines;
    std::string line;
    size_t w = 0;
    while (w < words.size()) {
        const std::string candidate = line.empty() ? words[w] : line + " " + words[w];
        if (measure(candidate) <= maxW) {
            line = candidate;
            ++w;
            continue;
        }
        if (line.empty()) {  // a single word wider than the line
            const size_t n = fittingPrefix(words[w], maxW, measure);
            line = words[w].substr(0, n);
            words[w] = words[w].substr(n);
        }
        if (static_cast<int>(lines.size()) + 1 == maxLines) {
            lines.push_back(ellipsize(line + " " + words[w], maxW, measure));
            return lines;
        }
        lines.push_back(line);
        line.clear();
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
