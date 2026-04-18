#ifndef HOURLY_PAGE_H
#define HOURLY_PAGE_H

#include "../IPage.h"
#include <DisplayManager.h>

class HourlyPage : public IPage {
    PageState _state = {};

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
        if (!_state.weather.valid) return;
        DisplayManager::getInstance().showHourlyPage(_state.weather);
    }

    bool handleTouch(int16_t /*x*/, int16_t /*y*/) override {
        return false;
    }
};

#endif // HOURLY_PAGE_H
