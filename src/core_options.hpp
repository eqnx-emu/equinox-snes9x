#pragma once

// Values the runner gives the Snes9x core options it answers. nullptr: not answered,
// the core keeps its default. The core's own NTSC filter stays off: NTSC is a shader.
const char* coreOptionValue(const char* key);
