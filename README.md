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

## Protocol

Equinox starts the runner by chain-loading `equinox-snes9x.nro` with this argument
line (protocol version 1):

    --protocol 1 --rom <path> --title <title> --saves <dir> --states <dir>
    --return <equinox.nro> --resume <token>

| Argument     | Required | Meaning |
|--------------|----------|---------|
| `--protocol` | yes      | Must be `1`; any other version is rejected. |
| `--rom`      | yes      | Path of the SNES ROM to run. |
| `--title`    | no       | Display name of the game. |
| `--saves`    | no       | Directory for battery saves. |
| `--states`   | no       | Directory for save states. |
| `--return`   | yes      | Path of the Equinox `.nro` to reopen on exit. |
| `--resume`   | no       | Opaque token. The runner never interprets it; Equinox decides what it holds (screen, filter, game) and gets it back unchanged. |

Without a valid line (opened from hbmenu) the runner shows "Abra este emulador pelo
Equinox" for 4 seconds and exits without scheduling a return.

On exit the runner calls `envSetNextLoad` with `"<return>" --resume "<token>"`, so
Equinox reopens with `--resume <token>`. libnx splits that string on spaces with
double-quote grouping and no escapes, so double quotes and control characters are
stripped from the values.
