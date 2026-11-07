#pragma once

enum class MenuItem { Continue, SaveState, LoadState, Video, Exit };
enum class MenuAction { None, Close, SaveState, LoadState, OpenVideo, Exit };

// In-game menu state. Without a saved state "Load state" is disabled and skipped by up/down.
class Menu {
public:
    bool isOpen() const { return open_; }
    void open();
    void close() { open_ = false; }
    void setHasState(bool has);
    bool hasState() const { return hasState_; }
    MenuItem selected() const { return sel_; }

    MenuAction up();
    MenuAction down();
    MenuAction confirm();
    MenuAction back();  // same as Close

private:
    void move(int dir);
    bool enabled(MenuItem i) const { return i != MenuItem::LoadState || hasState_; }
    bool open_ = false;
    bool hasState_ = false;
    MenuItem sel_ = MenuItem::Continue;
};
