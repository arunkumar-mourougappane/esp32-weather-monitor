#ifndef FORECAST_PAGE_H
#define FORECAST_PAGE_H

#include "../IPage.h"
#include <DisplayManager.h>

class ForecastPage : public IPage {
    SystemState _state = {};

    static constexpr int kTriTop   = 820;
    static constexpr int kTriBot   = 860;
    static constexpr int kTriLeft  =  60;
    static constexpr int kTriRight = 480;

public:
    void init()        override {}
    void onBlur()      override {}

    void onFocus() override {
        DisplayManager::getInstance().clear();
    }

    void updateData(const SystemState& state) override {
        _state = state;
    }

    void render() override {
        if (!_state.weather.valid) return;
        DisplayManager::getInstance().drawPageForecast(
            _state.weather, _state.forecastOffset);
    }

    bool handleTouch(int16_t x, int16_t y) override {
        if (y < kTriTop || y > kTriBot) return false;
        if (x < kTriLeft) {
            _lastAction = PageAction::DecrementForecastOffset;
            return true;
        }
        if (x > kTriRight) {
            _lastAction = PageAction::IncrementForecastOffset;
            return true;
        }
        return false;
    }
};

#endif // FORECAST_PAGE_H
