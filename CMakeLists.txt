cmake_minimum_required(VERSION 3.16)

# Two targets:
#   equinox-snes9x  the Switch .nro (built with the devkitPro toolchain file)
#   eqx_tests       host unit tests, with -DEQX_HOST_TESTS=ON

project(equinox-snes9x LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_ALTER "0")
set(APP_TITLE "Equinox SNES")
set(APP_AUTHOR "Equinox")
set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)

option(EQX_HOST_TESTS "Build the host unit tests" OFF)

file(GLOB_RECURSE EQX_ALL_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
set(EQX_LOGIC_SRC ${EQX_ALL_SRC})
list(FILTER EQX_LOGIC_SRC EXCLUDE REGEX "/src/platform/")

if (EQX_HOST_TESTS)
    enable_testing()
    file(GLOB EQX_TEST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    add_executable(eqx_tests ${EQX_LOGIC_SRC} ${EQX_TEST_SRC})
    target_include_directories(eqx_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/extern)
    add_test(NAME eqx_tests COMMAND eqx_tests)
endif ()

if (NINTENDO_SWITCH)
    set(SNES9X_LIB ${CMAKE_CURRENT_SOURCE_DIR}/extern/snes9x/libretro/snes9x_libretro_libnx.a)

    set(PORTLIBS_SWITCH /opt/devkitpro/portlibs/switch)

    add_executable(equinox-snes9x ${EQX_ALL_SRC})
    target_link_directories(equinox-snes9x PRIVATE ${PORTLIBS_SWITCH}/lib)
    target_include_directories(equinox-snes9x PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PORTLIBS_SWITCH}/include
        ${PORTLIBS_SWITCH}/include/SDL2
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/snes9x/libretro
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/snes9x/libretro/libretro-common/include)
    target_link_libraries(equinox-snes9x PRIVATE
        ${SNES9X_LIB} SDL2_ttf SDL2 freetype png bz2 z EGL glapi drm_nouveau nx)

    nx_generate_nacp(equinox-snes9x.nacp NAME "${APP_TITLE}" AUTHOR "${APP_AUTHOR}" VERSION "${APP_VERSION}")
    nx_create_nro(equinox-snes9x NACP equinox-snes9x.nacp ICON ${APP_ICON})
endif ()
