#ifndef DASHBOARD_PAGE_H
#define DASHBOARD_PAGE_H

#include "../IPage.h"
#include <DisplayManager.h>

class DashboardPage : public IPage {
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
        DisplayManager::getInstance().drawPageDashboard(
            _state.weather, _state.localTime, _state.city);
    }

    bool handleTouch(int16_t /*x*/, int16_t /*y*/) override {
        return false;  // Pagination dots handled by AppController/PageRouter.
    }
};

#endif // DASHBOARD_PAGE_H
