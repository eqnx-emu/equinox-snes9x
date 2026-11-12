#pragma once

#include <string>

// ROM file name without folder and without the last extension (the .zip, not the
// entry inside it). Accepts '/' and '\' separators.
std::string romStem(const std::string& romPath);
std::string srmPath(const std::string& savesDir, const std::string& romPath);    // <saves>/<stem>.srm
std::string statePath(const std::string& statesDir, const std::string& romPath);  // <states>/<stem>.state
std::string stateThumbPath(const std::string& statesDir, const std::string& romPath);  // <states>/<stem>.state.png
std::string videoPrefsPath(const std::string& savesDir, const std::string& romPath);  // <saves>/<stem>.video.json
