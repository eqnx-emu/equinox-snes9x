#pragma once

#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "strings.hpp"

// "Jogando há 42 min" / "Playing for 42 min"; an hour or more reads "1 h 05 min".
std::string playedFor(Lang lang, unsigned long seconds);

// "Hoje às 14:32", "Ontem às 09:05", else "28/10 às 23:00" (en-US: "10/28 at 23:00").
// Both times are local calendar times; only year, yday, mon, mday, hour, min are read.
std::string savedAt(Lang lang, const std::tm& when, const std::tm& now);

// Greedy word wrap into at most maxLines lines no wider than maxW, as measured. Text
// that does not fit ends its last line with "..."; a word wider than a line is cut.
std::vector<std::string> wrapLines(const std::string& text, int maxW,
                                   const std::function<int(const std::string&)>& measure, int maxLines);
