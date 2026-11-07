#pragma once

#include "strings.hpp"
#include "video_prefs.hpp"

enum class VideoRow { Size, Smooth, Filter, Border, UseDefault, Back };
enum class VideoAction { None, Changed, UseDefault, Close };

// State of the video options screen. No drawing, no I/O.
class VideoMenu {
public:
    explicit VideoMenu(VideoSettings current, bool ntscAllowed = kNtscEnabled)
        : s_(current), ntscAllowed_(ntscAllowed) {}

    VideoRow cursor() const { return cursor_; }
    const VideoSettings& settings() const { return s_; }
    void reset(VideoSettings s) { s_ = s; }  // after "Use default"
    bool rowEnabled(VideoRow r) const { return r != VideoRow::Border || s_.size != Size::Full; }

    void moveCursor(int dir);     // skips disabled rows, does not wrap
    VideoAction change(int dir);  // left/right on the first four rows, wraps
    VideoAction confirm();        // A: buttons act; value rows behave as change(+1)
    VideoAction back() { return VideoAction::Close; }

private:
    VideoSettings s_;
    bool ntscAllowed_;
    VideoRow cursor_ = VideoRow::Size;
};

Str label(Size);
Str label(Smooth);
Str label(Filter);
Str label(Border);
