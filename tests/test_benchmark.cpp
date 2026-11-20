#include "benchmark.hpp"

#include <string>

#include "doctest/doctest.h"

TEST_CASE("benchSteps: no filter first, then every filter at every intensity") {
    const auto steps = benchSteps();
    REQUIRE(steps.size() == 22);
    CHECK(steps.front().filter == Filter::None);
    CHECK(steps.front().intensity == Intensity::Medium);
    CHECK(steps[1].filter == Filter::Scanlines);
    CHECK(steps[1].intensity == Intensity::Light);
    CHECK(steps[3].intensity == Intensity::Strong);
    CHECK(steps[4].filter == Filter::Ntsc);
    CHECK(steps.back().filter == Filter::CrtGeom);
    CHECK(steps.back().intensity == Intensity::Strong);
}

TEST_CASE("FrameStats: mean, p99 and max") {
    FrameStats st;
    for (int i = 100; i >= 1; --i) st.add(i);  // out of order: p99 must sort
    CHECK(st.count() == 100);
    CHECK(st.mean() == doctest::Approx(50.5));
    CHECK(st.p99() == doctest::Approx(99));
    CHECK(st.max() == doctest::Approx(100));
}

TEST_CASE("FrameStats: empty is all zero") {
    const FrameStats st;
    CHECK(st.count() == 0);
    CHECK(st.mean() == 0);
    CHECK(st.p99() == 0);
    CHECK(st.max() == 0);
}

TEST_CASE("reportLine: columns, two decimals and the verdict against the budget") {
    FrameStats fast;
    fast.add(16.0);
    CHECK(reportLine({Filter::ZfastCrt, Intensity::Strong}, fast) ==
          "zfast-crt\tstrong\t1\t16.00\t16.00\t16.00\tok\n");

    FrameStats slow;
    slow.add(16.8);
    const std::string line = reportLine({Filter::ZfastCrt, Intensity::Strong}, slow);
    CHECK(line.size() > 6);
    CHECK(line.substr(line.size() - 6) == "\tslow\n");
}

TEST_CASE("reportLine: no frames after the warmup is no-data, not ok") {
    CHECK(reportLine({Filter::CrtGeom, Intensity::Light}, FrameStats{}) ==
          "crt-geom\tlight\t0\t0.00\t0.00\t0.00\tno-data\n");
}

TEST_CASE("unavailableLine: a filter that did not load is marked, not timed") {
    CHECK(unavailableLine({Filter::Ntsc, Intensity::Strong}) == "ntsc\tstrong\t0\t-\t-\t-\tunavailable\n");
}

TEST_CASE("reportHeader: comment lines with version, title and the column names") {
    const std::string h = reportHeader("1.4.0", "Super Metroid");
    REQUIRE_FALSE(h.empty());
    CHECK(h.front() == '#');
    CHECK(h.find("1.4.0") != std::string::npos);
    CHECK(h.find("Super Metroid") != std::string::npos);
    CHECK(h.find("filter\tintensity\tframes\tmean_ms\tp99_ms\tmax_ms\tresult\n") != std::string::npos);
}
