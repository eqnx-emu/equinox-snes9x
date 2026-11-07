#pragma once

enum class Lang { PtBR, EnUS };
enum class Str {
    Continue, SaveState, LoadState, Exit, StateSaved, StateLoaded,
    SaveFailed, NoState, OpenFromEquinox, RomNotFound, RomUnreadable,
    NoRomInZip, RomUnsupported, BackToEquinox, RomTooBig
};

// UTF-8 text, never null.
const char* tr(Lang lang, Str s);
