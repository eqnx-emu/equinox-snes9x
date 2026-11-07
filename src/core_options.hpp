#pragma once

// Values the runner gives the Snes9x core options it answers. nullptr: not answered,
// the core keeps its default.
const char* coreOptionValue(const char* key, bool ntsc);
