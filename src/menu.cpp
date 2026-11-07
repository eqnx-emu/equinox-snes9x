#include "menu.hpp"

namespace {
constexpr int kCount = 4;
}

void Menu::open() {
    open_ = true;
    sel_ = MenuItem::Continue;
}

void Menu::setHasState(bool has) {
    hasState_ = has;
    if (!enabled(sel_)) sel_ = MenuItem::SaveState;
}

void Menu::move(int dir) {
    int i = static_cast<int>(sel_);
    do {
        i = (i + dir + kCount) % kCount;
    } while (!enabled(static_cast<MenuItem>(i)));
    sel_ = static_cast<MenuItem>(i);
}

MenuAction Menu::up() {
    if (open_) move(-1);
    return MenuAction::None;
}

MenuAction Menu::down() {
    if (open_) move(1);
    return MenuAction::None;
}

MenuAction Menu::confirm() {
    if (!open_) return MenuAction::None;
    switch (sel_) {
        case MenuItem::Continue: open_ = false; return MenuAction::Close;
        case MenuItem::SaveState: return MenuAction::SaveState;
        case MenuItem::LoadState: return hasState_ ? MenuAction::LoadState : MenuAction::None;
        case MenuItem::Exit: return MenuAction::Exit;
    }
    return MenuAction::None;
}

MenuAction Menu::back() {
    if (!open_) return MenuAction::None;
    open_ = false;
    return MenuAction::Close;
}
