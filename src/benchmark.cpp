#include "benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

std::vector<BenchStep> benchSteps() {
    std::vector<BenchStep> steps{{Filter::None, Intensity::Medium}};
    for (int f = static_cast<int>(Filter::Scanlines); f <= static_cast<int>(Filter::CrtGeom); ++f)
        for (Intensity i : {Intensity::Light, Intensity::Medium, Intensity::Strong})
            steps.push_back({static_cast<Filter>(f), i});
    return steps;
}

void FrameStats::add(double ms) { ms_.push_back(ms); }

double FrameStats::mean() const {
    return ms_.empty() ? 0 : std::accumulate(ms_.begin(), ms_.end(), 0.0) / ms_.size();
}

double FrameStats::p99() const {
    if (ms_.empty()) return 0;
    std::vector<double> sorted = ms_;
    const size_t k = static_cast<size_t>(std::ceil(0.99 * sorted.size())) - 1;
    std::nth_element(sorted.begin(), sorted.begin() + k, sorted.end());
    return sorted[k];
}

double FrameStats::max() const { return ms_.empty() ? 0 : *std::max_element(ms_.begin(), ms_.end()); }

std::string reportLine(const BenchStep& s, const FrameStats& st) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s\t%s\t%zu\t%.2f\t%.2f\t%.2f\t%s\n", toId(s.filter), toId(s.intensity),
                  st.count(), st.mean(), st.p99(), st.max(),
                  st.count() == 0 ? "no-data" : st.p99() < kFrameBudgetMs ? "ok" : "slow");
    return buf;
}

std::string unavailableLine(const BenchStep& s) {
    return std::string(toId(s.filter)) + "\t" + toId(s.intensity) + "\t0\t-\t-\t-\tunavailable\n";
}

std::string reportHeader(const std::string& version, const std::string& title) {
    char budget[96];
    std::snprintf(budget, sizeof budget, "# %.0f s per step, first %.0f s dropped, ok when p99 < %.1f ms\n",
                  kBenchStepSec, kBenchWarmupSec, kFrameBudgetMs);
    return "# equinox runner " + version + " benchmark\n# game: " + title + "\n" + budget +
           "filter\tintensity\tframes\tmean_ms\tp99_ms\tmax_ms\tresult\n";
}
