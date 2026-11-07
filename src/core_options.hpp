#pragma once

#include "video_prefs.hpp"

// Values the runner gives the Snes9x core options it answers. nullptr: not answered,
// the core keeps its default.
const char* coreOptionValue(const char* key, bool ntsc);

// Whether a video filter is the core's NTSC composite filter.
bool ntscFor(Filter filter);
