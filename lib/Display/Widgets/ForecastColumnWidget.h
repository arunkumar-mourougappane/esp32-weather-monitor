/**
 * @file ForecastColumnWidget.h
 * @brief Single-day forecast column widget.
 *
 * Renders one vertical column of the 10-day forecast view:
 *   - Day label (weekday + date, or "Today")
 *   - Weather icon (52 px)
 *   - Condition text (truncated at 12 chars)
 *   - Precipitation-type badge (Rain / Snow / Storm / Ice / Hail)
 *   - H / L temperature line
 *   - Temperature range bar scaled to the 10-day global min/max span
 *   - Precipitation chance
 *
 * globalMin / globalMax must be provided by the caller (the Page/Controller
 * that owns the three ForecastColumnWidgets) so the range bars are comparable
 * across columns on the same screen.
 *
 * Layout geometry matches drawPageForecast() so this widget is a drop-in
 * replacement for the inline column-drawing loop.
 */
#ifndef FORECAST_COLUMN_WIDGET_H
#define FORECAST_COLUMN_WIDGET_H

#include "../Widget.h"
#include <WeatherService.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

class ForecastColumnWidget : public Widget {
    DailyForecast _day    = {};
    bool          _isToday = false;
    float         _globalMin = 0.0f;
    float         _globalMax = 1.0f;

public:
    ForecastColumnWidget(IDisplayController* gfx,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h)
        : Widget(gfx, x, y, w, h) {}

    /**
     * @brief Feed a new forecast day entry.
     * @param day        Forecast data for this column's day.
     * @param isToday    When true the day label reads "Today" instead of weekday.
     * @param globalMin  10-day low temperature (for the range bar baseline).
     * @param globalMax  10-day high temperature (for the range bar ceiling).
     */
    void update(const DailyForecast& day, bool isToday,
                float globalMin, float globalMax) {
        bool changed = (isToday            != _isToday)
                    || (day.dayTime        != _day.dayTime)
                    || (day.maxTempC       != _day.maxTempC)
                    || (day.minTempC       != _day.minTempC)
                    || (day.precipChance   != _day.precipChance)
                    || (strncmp(day.condition, _day.condition,
                                sizeof(_day.condition) - 1) != 0);
        if (changed) {
            _day       = day;
            _isToday   = isToday;
            _globalMin = globalMin;
            _globalMax = (globalMax - globalMin < 1.0f) ? globalMin + 1.0f : globalMax;
            markDirty();
        }
    }

    void draw() override {
        _gfx->fillRect(_x, _y, _w, _h, TFT_WHITE);

        int cx = _x + _w / 2;
        _gfx->setTextColor(TFT_BLACK);
        _gfx->setTextDatum(WidgetDatum::TopCenter);

        // ── Day label ────────────────────────────────────────────────────────
        _gfx->setFont(WidgetFont::Medium);
        _gfx->setTextSize(1.0f);
        if (_isToday) {
            _gfx->drawString("Today", cx, _y + 8);
        } else if (_day.dayTime > 0) {
            struct tm dayTm;
            time_t dt = _day.dayTime;
            localtime_r(&dt, &dayTm);
            char dayBuf[12];
            strftime(dayBuf, sizeof(dayBuf), "%a %d", &dayTm);
            _gfx->drawString(dayBuf, cx, _y + 8);
        } else {
            _gfx->drawString("--", cx, _y + 8);
        }

        // ── Weather icon ─────────────────────────────────────────────────────
        int iconY = _y + 58;
        _gfx->drawWeatherIcon(_day.condition, cx, iconY, 52);

        // ── Condition text (truncated) ────────────────────────────────────────
        _gfx->setFont(WidgetFont::Small);
        char condBuf[16];
        size_t condLen = strlen(_day.condition);
        if (condLen > 12) {
            strncpy(condBuf, _day.condition, 10);
            condBuf[10] = '.'; condBuf[11] = '.'; condBuf[12] = '\0';
        } else {
            strncpy(condBuf, _day.condition, sizeof(condBuf) - 1);
            condBuf[sizeof(condBuf) - 1] = '\0';
        }
        _gfx->drawString(condBuf, cx, _y + 104);

        // ── Precipitation-type badge ──────────────────────────────────────────
        {
            String condLower = _day.condition;
            condLower.toLowerCase();
            const char* badge = nullptr;
            if      (condLower.indexOf("thunder") >= 0 || condLower.indexOf("storm") >= 0) badge = "Storm";
            else if (condLower.indexOf("hail")    >= 0)                                     badge = "Hail";
            else if (condLower.indexOf("freezing") >= 0 || condLower.indexOf("sleet") >= 0
                     || condLower.indexOf("ice")  >= 0 || condLower.indexOf("pellet") >= 0) badge = "Ice";
            else if (condLower.indexOf("snow")    >= 0 || condLower.indexOf("blizzard") >= 0) badge = "Snow";
            else if (condLower.indexOf("rain")    >= 0 || condLower.indexOf("shower") >= 0
                     || condLower.indexOf("drizzle") >= 0)                                  badge = "Rain";
            if (badge) {
                int badgeX = cx - 18;
                int badgeY = _y + 122;
                _gfx->fillRect(badgeX, badgeY, 36, 13, TFT_BLACK);
                _gfx->setTextColor(TFT_WHITE);
                _gfx->drawString(badge, cx, badgeY + 1);
                _gfx->setTextColor(TFT_BLACK);
            }
        }

        // ── H / L temperatures ────────────────────────────────────────────────
        char tempBuf[24];
        snprintf(tempBuf, sizeof(tempBuf), "H:%.0f  L:%.0f", _day.maxTempC, _day.minTempC);
        _gfx->drawString(tempBuf, cx, _y + 138);

        // ── Temperature range bar (relative to 10-day span) ───────────────────
        constexpr int barW = 100, barH = 7;
        int barX = cx - barW / 2;
        int barY = _y + 158;
        float range = _globalMax - _globalMin;
        _gfx->drawRect(barX, barY, barW, barH, TFT_BLACK);
        int fillStart = static_cast<int>((_day.minTempC - _globalMin) / range * barW);
        int fillEnd   = static_cast<int>((_day.maxTempC - _globalMin) / range * barW);
        int fillW     = std::max(4, fillEnd - fillStart);
        _gfx->fillRect(barX + fillStart, barY, fillW, barH, TFT_BLACK);

        // ── Precipitation chance ──────────────────────────────────────────────
        char popBuf[16];
        snprintf(popBuf, sizeof(popBuf), "Precip: %d%%", _day.precipChance);
        _gfx->drawString(popBuf, cx, _y + 178);

        _gfx->setTextDatum(WidgetDatum::TopLeft);
        clearDirty();
    }
};

#endif // FORECAST_COLUMN_WIDGET_H
