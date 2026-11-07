#pragma once

#include "args.hpp"

// Schedules Equinox to open when this process exits. False when the loader
// (hbmenu) does not support chain-loading.
bool scheduleReturn(const LaunchArgs& args);
