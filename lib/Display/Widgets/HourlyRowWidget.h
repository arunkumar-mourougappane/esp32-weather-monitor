/**
 * @file HourlyRowWidget.h
 * @brief Single hourly-forecast cell widget.
 *
 * Renders one cell of the 4 × 6 hourly grid used by showHourlyPage():
 *   - 12-hour time string
 *   - Weather icon (36 px) mapped from the WMO weather code
 *   - Temperature in °C
 *   - Precipitation chance (omitted when 0)
 *   - Wind speed in km/h (omitted when ≤ 0.5)
 *
 * Marks dirty only when any data field changes, so scrolling or rotating to
 * a different page and back causes a full redraw, but a static hourly grid
 * does not re-render unchanged cells.
 *
 * The WMO → condition mapping matches the one in DisplayManager::showHourlyPage().
 */
#ifndef HOURLY_ROW_WIDGET_H
#define HOURLY_ROW_WIDGET_H

#include "../Widget.h"
#include <WeatherService.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

class HourlyRowWidget : public Widget {
    HourlyForecast _hour = {};

    static const char* _wmoToCondition(int code) {
        if (code == 0)                    return "Clear";
        if (code <= 3)                    return "Partly Cloudy";
        if (code == 45 || code == 48)     return "Fog";
        if (code == 51 || code == 53)     return "Drizzle";
        if (code == 55)                   return "Drizzle";
        if (code == 56 || code == 57)     return "Freezing Drizzle";
        if (code == 61 || code == 63)     return "Rain";
        if (code == 65)                   return "Heavy Showers";
        if (code == 66 || code == 67)     return "Freezing Rain";
        if (code == 71 || code == 73)     return "Snow";
        if (code == 75)                   return "Heavy Snow";
        if (code == 77)                   return "Snow";
        if (code == 80 || code == 81)     return "Rain";
        if (code == 82)                   return "Heavy Showers";
        if (code == 85 || code == 86)     return "Snow Showers";
        if (code == 95)                   return "Thunderstorms";
        if (code == 96 || code == 99)     return "Thunderstorms";
        return "Mostly Cloudy";
    }

public:
    HourlyRowWidget(IDisplayController* gfx,
                    int16_t x, int16_t y,
                    uint16_t w, uint16_t h)
        : Widget(gfx, x, y, w, h) {}

    /**
     * @brief Feed one HourlyForecast entry; marks dirty when any field changes.
     */
    void update(const HourlyForecast& hour) {
        bool changed = (_hour.timestamp    != hour.timestamp)
                    || (_hour.tempC        != hour.tempC)
                    || (_hour.precipChance != hour.precipChance)
                    || (_hour.windKph      != hour.windKph)
                    || (_hour.weatherCode  != hour.weatherCode);
        if (changed) {
            _hour = hour;
            markDirty();
        }
    }

    void draw() override {
        _gfx->fillRect(_x, _y, _w, _h, TFT_WHITE);

        int cx = _x + _w / 2;

        // ── Time (12-hour, leading space trimmed) ─────────────────────────────
        struct tm* ti = localtime(&_hour.timestamp);
        char timeBuf[16];
        strftime(timeBuf, sizeof(timeBuf), "%l:%M%p", ti);
        const char* trimmed = (timeBuf[0] == ' ') ? timeBuf + 1 : timeBuf;

        _gfx->setFont(WidgetFont::Small);
        _gfx->setTextSize(1.0f);
        _gfx->setTextColor(TFT_BLACK);
        _gfx->setTextDatum(WidgetDatum::TopCenter);
        _gfx->drawString(trimmed, cx, _y);

        // ── Weather icon ──────────────────────────────────────────────────────
        const char* cond = _wmoToCondition(_hour.weatherCode);
        _gfx->drawWeatherIcon(cond, cx, _y + 30, 36);

        // ── Temperature ───────────────────────────────────────────────────────
        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%.0f C", _hour.tempC);
        _gfx->drawString(tempBuf, cx, _y + 55);

        // ── Precipitation % (hidden when 0) ───────────────────────────────────
        if (_hour.precipChance > 0) {
            char popBuf[16];
            snprintf(popBuf, sizeof(popBuf), "%d%%", _hour.precipChance);
            _gfx->drawString(popBuf, cx, _y + 75);
        }

        // ── Wind speed (hidden when calm) ─────────────────────────────────────
        if (_hour.windKph > 0.5f) {
            char wBuf[16];
            snprintf(wBuf, sizeof(wBuf), "%.0fkm/h", _hour.windKph);
            _gfx->drawString(wBuf, cx, _y + 93);
        }

        _gfx->setTextDatum(WidgetDatum::TopLeft);
        clearDirty();
    }
};

#endif // HOURLY_ROW_WIDGET_H
