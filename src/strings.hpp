#pragma once

enum class Lang { PtBR, EnUS };
enum class Str {
    Continue, SaveState, LoadState, Exit, StateSaved, StateLoaded,
    SaveFailed, NoState, OpenFromEquinox, RomNotFound, RomUnreadable,
    NoRomInZip, RomUnsupported, BackToEquinox, RomTooBig,
    Video, VideoTitle, Size, Smoothing, Filter, Border, UseDefault, Back,
    SizeOriginal, Size87, Size43, SizeFull, SmoothPixel, SmoothSharp, SmoothSoft,
    FilterNone, FilterScanlines, FilterNtsc, BorderEquinox, BorderCover, BorderTv,
    BorderBlack, VideoSaveFailed
};

// UTF-8 text, never null.
const char* tr(Lang lang, Str s);
