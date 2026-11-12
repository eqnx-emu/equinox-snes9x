#pragma once

#include "strings.hpp"
#include "video_prefs.hpp"

enum class VideoRow { Size, Smooth, Filter, Intensity, Border, UseDefault, Back };
enum class VideoAction { None, Changed, UseDefault, Close };

// State of the video options screen. No drawing, no I/O.
class VideoMenu {
public:
    explicit VideoMenu(VideoSettings current) : s_(current) {}

    VideoRow cursor() const { return cursor_; }
    const VideoSettings& settings() const { return s_; }
    void reset(VideoSettings s) { s_ = s; }  // after "Use default"
    // A shader filter does its own sampling, so smoothing only applies without one;
    // intensity only means something with one. A disabled row keeps its value.
    bool rowEnabled(VideoRow r) const {
        switch (r) {
            case VideoRow::Smooth: return s_.filter == Filter::None;
            case VideoRow::Intensity: return s_.filter != Filter::None;
            case VideoRow::Border: return s_.size != Size::Full;
            default: return true;
        }
    }

    void moveCursor(int dir);     // skips disabled rows, does not wrap
    VideoAction change(int dir);  // left/right on the value rows, wraps
    VideoAction confirm();        // A: buttons act; value rows behave as change(+1)
    VideoAction back() { return VideoAction::Close; }

private:
    VideoSettings s_;
    VideoRow cursor_ = VideoRow::Size;
};

Str label(Size);
Str label(Smooth);
Str label(Filter);
Str label(Border);
Str label(Intensity);
