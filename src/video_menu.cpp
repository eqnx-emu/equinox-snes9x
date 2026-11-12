#include "video_menu.hpp"

namespace {
template <typename E>
E step(E v, int dir, int count) {
    return static_cast<E>((static_cast<int>(v) + (dir < 0 ? -1 : 1) + count) % count);
}
}  // namespace

void VideoMenu::moveCursor(int dir) {
    const int d = dir < 0 ? -1 : 1;
    const bool onButtons = cursor_ == VideoRow::UseDefault || cursor_ == VideoRow::Back;
    if (onButtons && d > 0) return;
    int i = (onButtons ? static_cast<int>(VideoRow::UseDefault) : static_cast<int>(cursor_)) + d;
    if (i == static_cast<int>(VideoRow::Back)) i = static_cast<int>(VideoRow::UseDefault);
    while (i >= static_cast<int>(VideoRow::Size) && i <= static_cast<int>(VideoRow::Back)) {
        if (rowEnabled(static_cast<VideoRow>(i))) {
            cursor_ = static_cast<VideoRow>(i);
            return;
        }
        i += d;
    }
}

VideoAction VideoMenu::change(int dir) {
    switch (cursor_) {
        case VideoRow::Size: s_.size = step(s_.size, dir, 4); return VideoAction::Changed;
        case VideoRow::Smooth: s_.smooth = step(s_.smooth, dir, 3); return VideoAction::Changed;
        case VideoRow::Filter: s_.filter = step(s_.filter, dir, kFilterCount); return VideoAction::Changed;
        case VideoRow::Intensity: s_.intensity = step(s_.intensity, dir, 3); return VideoAction::Changed;
        case VideoRow::Border:
            s_.border = step(s_.border, dir, 4);
            // The TV frame only fits around Original; elsewhere it would draw black sides.
            if (s_.border == Border::Tv && s_.size != Size::Original) s_.border = step(s_.border, dir, 4);
            return VideoAction::Changed;
        case VideoRow::UseDefault:
        case VideoRow::Back: cursor_ = dir < 0 ? VideoRow::UseDefault : VideoRow::Back; break;
    }
    return VideoAction::None;
}

VideoAction VideoMenu::confirm() {
    switch (cursor_) {
        case VideoRow::UseDefault: return VideoAction::UseDefault;
        case VideoRow::Back: return VideoAction::Close;
        default: return change(1);
    }
}

Str label(Size v) {
    switch (v) {
        case Size::Original: return Str::SizeOriginal;
        case Size::Aspect87: return Str::Size87;
        case Size::Aspect43: return Str::Size43;
        case Size::Full: return Str::SizeFull;
    }
    return Str::SizeOriginal;
}

Str label(Smooth v) {
    switch (v) {
        case Smooth::Pixel: return Str::SmoothPixel;
        case Smooth::Sharp: return Str::SmoothSharp;
        case Smooth::Soft: return Str::SmoothSoft;
    }
    return Str::SmoothPixel;
}

Str label(Filter v) {
    switch (v) {
        case Filter::None: return Str::FilterNone;
        case Filter::Scanlines: return Str::FilterScanlines;
        case Filter::Ntsc: return Str::FilterNtsc;
        case Filter::ZfastCrt: return Str::FilterZfastCrt;
        case Filter::CrtEasymode: return Str::FilterCrtEasymode;
        case Filter::CrtLottes: return Str::FilterCrtLottes;
        case Filter::CrtHyllian: return Str::FilterCrtHyllian;
        case Filter::CrtGeom: return Str::FilterCrtGeom;
    }
    return Str::FilterNone;
}

Str label(Border v) {
    switch (v) {
        case Border::Equinox: return Str::BorderEquinox;
        case Border::Cover: return Str::BorderCover;
        case Border::Tv: return Str::BorderTv;
        case Border::Black: return Str::BorderBlack;
    }
    return Str::BorderEquinox;
}

Str label(Intensity v) {
    switch (v) {
        case Intensity::Light: return Str::IntensityLight;
        case Intensity::Medium: return Str::IntensityMedium;
        case Intensity::Strong: return Str::IntensityStrong;
    }
    return Str::IntensityMedium;
}

Str description(Filter v) {
    switch (v) {
        case Filter::None: return Str::DescNone;
        case Filter::Scanlines: return Str::DescScanlines;
        case Filter::Ntsc: return Str::DescNtsc;
        case Filter::ZfastCrt: return Str::DescZfastCrt;
        case Filter::CrtEasymode: return Str::DescCrtEasymode;
        case Filter::CrtLottes: return Str::DescCrtLottes;
        case Filter::CrtHyllian: return Str::DescCrtHyllian;
        case Filter::CrtGeom: return Str::DescCrtGeom;
    }
    return Str::DescNone;
}
