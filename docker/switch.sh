#!/usr/bin/env bash
# Builds equinox-snes9x.nro inside the devkitPro container.
# The CMake build tree lives in a Docker volume (/build) because bind mounts on
# Windows are slow; only the final files are copied back to the repo.
set -euo pipefail

SRC=/src
BUILD=/build/switch
OUT="$SRC/build/docker"
JOBS="${JOBS:-$(nproc)}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

git config --global --add safe.directory '*'

if [ ! -f "$SRC/extern/snes9x/libretro/Makefile" ]; then
    echo "Submodules are missing. Run: git submodule update --init --recursive" >&2
    exit 1
fi

# SDL2 and SDL2_ttf come from devkitPro portlibs.
for pkg in switch-sdl2 switch-sdl2_ttf switch-freetype switch-libpng switch-bzip2 switch-zlib; do
    dkp-pacman -Q "$pkg" >/dev/null 2>&1 || dkp-pacman -S --noconfirm "$pkg"
done

# Snes9x core as a static library: extern/snes9x/libretro/snes9x_libretro_libnx.a
make -C "$SRC/extern/snes9x/libretro" platform=libnx -j"$JOBS"

cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD" --target equinox-snes9x_nro -j"$JOBS"

mkdir -p "$OUT"
cp "$BUILD/equinox-snes9x.nro" "$OUT/equinox-snes9x.nro"
cp "$BUILD/equinox-snes9x.elf" "$OUT/equinox-snes9x.elf"
echo "Done: build/docker/equinox-snes9x.nro"
