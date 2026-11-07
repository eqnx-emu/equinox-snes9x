# equinox-snes9x

A standalone Nintendo Switch homebrew (`.nro`) that runs the
[Snes9x](https://github.com/libretro/snes9x) libretro core. It is launched by the
Equinox app (a Moonlight fork) to play SNES games.

## License note

Snes9x is distributed under a non-commercial license. For that reason this runner
is distributed separately from Equinox and is not bundled with it. Check the
Snes9x license (`extern/snes9x/LICENSE`) before redistributing any build.

## Build

Clone with submodules: `git clone --recurse-submodules ...`
(or `git submodule update --init`).

Switch `.nro` (needs Docker, uses the `devkitpro/devkita64` image):

    docker compose run --rm switch

Output: `build/docker/equinox-snes9x.nro`. The first run also installs
`switch-sdl2` and `switch-sdl2_ttf` from devkitPro portlibs.

Host unit tests:

    cmake -B build/host -DEQX_HOST_TESTS=ON
    cmake --build build/host
    ctest --test-dir build/host --output-on-failure
