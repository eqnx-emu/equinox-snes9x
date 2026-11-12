# Bundled shaders

Copied unchanged from [libretro/glsl-shaders](https://github.com/libretro/glsl-shaders),
commit `f277bd87e454a68c50e01e61e3588118d56b1365`, keeping the repository's paths.
They are read at run time from `romfs:/shaders/`; nothing in `src/` is derived from them.

| File | Author | License (from the file's header) |
|------|--------|----------------------------------|
| `crt/shaders/zfast_crt.glsl` | Greg Hogan (SoltanGris42) | GPL-2.0-or-later (`GPL-2.0.txt`) |
| `crt/shaders/crt-easymode.glsl` | EasyMode | "License: GPL" (`GPL-2.0.txt`) |
| `crt/shaders/crt-lottes-fast.glsl` | Timothy Lottes, adapted by hunterk | Unlicense (public domain) |
| `crt/shaders/hyllian/crt-hyllian.glsl` | Hyllian | MIT (text in the file's header) |
| `crt/shaders/crt-geom.glsl` | cgwg, Themaister and DOLLS | GPL-2.0-or-later (`GPL-2.0.txt`) |
| `scanlines/shaders/scanline.glsl` | libretro | No license header in the file |
| `ntsc/shaders/ntsc-adaptive/ntsc-pass1.glsl`, `ntsc-pass2.glsl` | based on Themaister's NTSC shader | No license header in the file |

The `.glslp` presets beside them are the repository's presets for these shaders.
