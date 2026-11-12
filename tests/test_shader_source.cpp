#include "shader_source.hpp"

#include <string>

#include "doctest/doctest.h"

TEST_CASE("a pragma parameter reads name, default and range") {
    const auto d = parseParameters("#pragma parameter MASK_DARK \"Mask Effect Amount\" 0.25 0.0 1.0 0.05\n");
    REQUIRE(d.size() == 1);
    CHECK(d[0].name == "MASK_DARK");
    CHECK(d[0].def == 0.25f);
    CHECK(d[0].min == 0.0f);
    CHECK(d[0].max == 1.0f);
}

TEST_CASE("extra spaces and quotes in the description") {
    const auto d = parseParameters(
        "void f();\n"
        "#pragma parameter   CURVATURE   \"CRT \\\"Curvature\\\" Toggle\"   1.0  0.0  1.0  1.0\n"
        "#pragma parameter x_tilt \"Horizontal Tilt\" 0.0 -0.5 0.5 0.05\n");
    REQUIRE(d.size() == 2);
    CHECK(d[0].name == "CURVATURE");
    CHECK(d[0].def == 1.0f);
    CHECK(d[0].max == 1.0f);
    CHECK(d[1].name == "x_tilt");
    CHECK(d[1].min == -0.5f);
}

TEST_CASE("a source without version builds a GLES2 fragment stage") {
    const auto s = buildStage("uniform float x;\nvoid main() {}\n", Stage::Fragment, false);
    REQUIRE(s);
    CHECK(s->rfind("#version 100\n", 0) == 0);
    const size_t frag = s->find("#define FRAGMENT");
    const size_t param = s->find("#define PARAMETER_UNIFORM");
    const size_t body = s->find("uniform float x;");
    CHECK(frag != std::string::npos);
    CHECK(param != std::string::npos);
    CHECK(frag < param);
    CHECK(param < body);
    CHECK(s->find("#define VERTEX") == std::string::npos);
}

TEST_CASE("version 120 becomes 100 and the original line goes away") {
    const auto s = buildStage("#version 120\nvoid main() {}\n", Stage::Vertex, true);
    REQUIRE(s);
    CHECK(s->rfind("#version 100\n", 0) == 0);
    CHECK(s->find("#version 120") == std::string::npos);
    CHECK(s->find("#define VERTEX") != std::string::npos);
}

TEST_CASE("version 130 needs GLES3") {
    const auto s = buildStage("#version 130\nvoid main() {}\n", Stage::Fragment, true);
    REQUIRE(s);
    CHECK(s->rfind("#version 300 es\n", 0) == 0);
    CHECK(s->find("#version 130") == std::string::npos);
    CHECK_FALSE(buildStage("#version 130\nvoid main() {}\n", Stage::Fragment, false));
}

TEST_CASE("a version after leading comments is still found") {
    const auto s = buildStage("// header\n\n#version 130\nvoid main() {}\n", Stage::Fragment, true);
    REQUIRE(s);
    CHECK(s->rfind("#version 300 es\n", 0) == 0);
    CHECK(s->find("#version 130") == std::string::npos);
    CHECK(s->find("// header") != std::string::npos);
}
