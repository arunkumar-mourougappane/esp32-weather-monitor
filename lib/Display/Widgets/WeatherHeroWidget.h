/**
 * @file WeatherHeroWidget.h
 * @brief Current-conditions hero widget — icon, temperature, and condition string.
 *
 * Marks dirty whenever temperature or condition text changes.  Mirrors the
 * hero section drawn by DisplayManager::drawPageDashboard() at Y ≈ 200–400.
 */
#ifndef WEATHER_HERO_WIDGET_H
#define WEATHER_HERO_WIDGET_H

#include "../Widget.h"
#include <stdio.h>
#include <string.h>

class WeatherHeroWidget : public Widget {
    float _tempC        = 0.0f;
    float _feelsLikeC   = 0.0f;
    char  _condition[64] = {};

public:
    /**
     * @param gfx  Drawing backend.
     * @param x,y  Top-left origin of the bounding box.
     * @param w,h  Bounding-box dimensions; default matches the Dashboard hero
     *             section (540 × 200 px, starting at Y = 200).
     */
    WeatherHeroWidget(IDisplayController* gfx,
                      int16_t x, int16_t y,
                      uint16_t w = 540, uint16_t h = 200)
        : Widget(gfx, x, y, w, h) {}

    /**
     * @brief Feed updated current-conditions data.
     * @param tempC      Current temperature in °C.
     * @param feelsLikeC Apparent temperature in °C (displayed as "Feels: X C").
     * @param condition  Sky-condition string (e.g. "Partly Cloudy").
     */
    void update(float tempC, float feelsLikeC, const char* condition) {
        bool changed = (_tempC      != tempC)
                    || (_feelsLikeC != feelsLikeC)
                    || (strncmp(_condition, condition, sizeof(_condition) - 1) != 0);
        if (changed) {
            _tempC      = tempC;
            _feelsLikeC = feelsLikeC;
            strncpy(_condition, condition, sizeof(_condition) - 1);
            _condition[sizeof(_condition) - 1] = '\0';
            markDirty();
        }
    }

    void draw() override {
        _gfx->fillRect(_x, _y, _w, _h, TFT_WHITE);

        // Weather icon — left third, vertically centred
        int iconX = _x + _w / 4;
        int iconY = _y + _h / 2;
        _gfx->drawWeatherIcon(_condition, iconX, iconY, 80);

        // Temperature — right of icon, aligned near top of icon
        char tempBuf[32];
        snprintf(tempBuf, sizeof(tempBuf), "%.1f C", _tempC);
        _gfx->setFont(WidgetFont::XLargeBold);
        _gfx->setTextSize(1.5f);
        _gfx->setTextColor(TFT_BLACK);
        _gfx->setTextDatum(WidgetDatum::TopLeft);
        _gfx->drawString(tempBuf, _x + _w / 2, _y + _h / 2 - 30);

        // Feels-like — smaller, just below temperature
        char feelsBuf[32];
        snprintf(feelsBuf, sizeof(feelsBuf), "Feels: %.1f C", _feelsLikeC);
        _gfx->setFont(WidgetFont::Medium);
        _gfx->setTextSize(1.0f);
        _gfx->drawString(feelsBuf, _x + _w / 2, _y + _h / 2 + 10);

        // Condition string — centred below icon + temp block
        _gfx->setFont(WidgetFont::XLarge);
        _gfx->setTextSize(1.0f);
        _gfx->setTextDatum(WidgetDatum::TopCenter);
        _gfx->drawString(_condition, _x + _w / 2, _y + _h - 45);

        _gfx->setTextDatum(WidgetDatum::TopLeft);
        clearDirty();
    }
};

#endif // WEATHER_HERO_WIDGET_H
