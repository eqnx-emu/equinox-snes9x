#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "video_prefs.hpp"

// Benchmark mode: with the marker file present when a game opens, the runner plays it
// through every filter and intensity with vsync off and logs the frame times.
inline constexpr const char* kBenchMarker = "sdmc:/equinox/emulators/snes/benchmark";
inline constexpr const char* kBenchLog = "sdmc:/equinox/emulators/snes/benchmark.log";
inline constexpr double kBenchWarmupSec = 2.0, kBenchStepSec = 10.0, kFrameBudgetMs = 16.7;

struct BenchStep {
    Filter filter;
    Intensity intensity;
};

// {None, Medium}, then each filter from Scanlines to CrtGeom at Light, Medium, Strong.
std::vector<BenchStep> benchSteps();

class FrameStats {
public:
    void add(double ms);
    size_t count() const { return ms_.size(); }
    double mean() const;
    double p99() const;  // element ceil(0.99 n) - 1 of the sorted times
    double max() const;

private:
    std::vector<double> ms_;
};

// "filter\tintensity\tframes\tmean_ms\tp99_ms\tmax_ms\tok|slow|no-data\n"; slow when
// p99 >= budget, no-data without frames.
std::string reportLine(const BenchStep& s, const FrameStats& st);
// The line for a step whose filter could not be built: nothing was timed.
std::string unavailableLine(const BenchStep& s);
// '#' comment lines (version, game, budget), then the column names.
std::string reportHeader(const std::string& version, const std::string& title);
