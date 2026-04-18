/**
 * @file ClockWidget.h
 * @brief Clock strip widget — time + optional NTP-failure badge.
 *
 * Marks dirty only when tm_hour or tm_min changes, or when the NTP failure
 * state flips, so the e-ink partial refresh fires at most once per minute
 * rather than on every render tick.
 *
 * Default bounding box: full-width clock strip (x=0, y=0, w=540, h=95),
 * matching the region used by DisplayManager::updateClockOnly().
 */
#ifndef CLOCK_WIDGET_H
#define CLOCK_WIDGET_H

#include "../Widget.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

class ClockWidget : public Widget {
    struct tm _time     = {};
    bool      _ntpFailed = false;

public:
    ClockWidget(IDisplayController* gfx,
                int16_t x, int16_t y, uint16_t w, uint16_t h)
        : Widget(gfx, x, y, w, h) {}

    /**
     * @brief Feed updated time; marks dirty only when the displayed minute changes.
     * @param t          Current local time.
     * @param ntpFailed  When true a small "NTP!" badge is rendered next to the time.
     */
    void update(const struct tm& t, bool ntpFailed = false) {
        if (_time.tm_hour  != t.tm_hour
         || _time.tm_min   != t.tm_min
         || _ntpFailed     != ntpFailed) {
            _time      = t;
            _ntpFailed = ntpFailed;
            markDirty();
        }
    }

    void draw() override {
        // Erase bounding box before redraw to prevent ghost remnants on e-ink.
        _gfx->fillRect(_x, _y, _w, _h, TFT_WHITE);

        char timeBuf[32], dateBuf[48];
        strftime(timeBuf, sizeof(timeBuf), "%l:%M %p", &_time);
        strftime(dateBuf, sizeof(dateBuf),  "%A, %B %d %Y", &_time);

        // Time string — large bold, centred in the widget
        _gfx->setFont(WidgetFont::XLargeBold);
        _gfx->setTextSize(1.5f);
        _gfx->setTextColor(TFT_BLACK);
        _gfx->setTextDatum(WidgetDatum::TopCenter);
        _gfx->drawString(timeBuf, _x + _w / 2, _y + 5);

        // NTP failure badge in top-right corner
        if (_ntpFailed) {
            _gfx->setFont(WidgetFont::Tiny);
            _gfx->setTextSize(1.0f);
            _gfx->setTextDatum(WidgetDatum::TopLeft);
            _gfx->drawString("NTP!", _x + _w - 44, _y + 22);
        }

        _gfx->setTextDatum(WidgetDatum::TopLeft);
        clearDirty();
    }
};

#endif // CLOCK_WIDGET_H
