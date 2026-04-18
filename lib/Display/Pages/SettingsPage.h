#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include "../IPage.h"
#include <DisplayManager.h>

class SettingsPage : public IPage {
    PageState _state = {};

    static constexpr int kTapTop   = 200;
    static constexpr int kTapBot   = 370;
    static constexpr int kColWidth = 180;

public:
    void init()        override {}
    void onBlur()      override {}

    void onFocus() override {
        DisplayManager::getInstance().clear();
    }

    void updateData(const PageState& state) override {
        _state = state;
    }

    void render() override {
        DisplayManager::getInstance().drawPageSettings(_state.settingsCursor);
    }

    bool handleTouch(int16_t x, int16_t y) override {
        if (y < kTapTop || y >= kTapBot) return false;
        int col = x / kColWidth;
        switch (col) {
            case 0: _lastAction = PageAction::ForceSync;         return true;
            case 1: _lastAction = PageAction::StartProvisioning; return true;
            case 2: _lastAction = PageAction::EnterSleep;        return true;
            default: return false;
        }
    }
};

#endif // SETTINGS_PAGE_H
