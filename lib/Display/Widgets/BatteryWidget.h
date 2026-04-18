/**
 * @file BatteryWidget.h
 * @brief Battery gauge widget — outline cell, fill bar, and percentage text.
 *
 * Marks dirty only when the level crosses a 5 % bucket boundary (or when the
 * charging state changes), so normal ADC noise does not trigger unnecessary
 * e-ink partial refreshes.
 *
 * Drawing geometry mirrors DisplayManager::_drawBattery() exactly so the two
 * can be swapped without visual change.
 *
 * Default bounding box: top-right corner, 100 × 25 px.
 */
#ifndef BATTERY_WIDGET_H
#define BATTERY_WIDGET_H

#include "../Widget.h"
#include <stdio.h>
#include <algorithm>

class BatteryWidget : public Widget {
    int  _level    = -1;   // -1 forces first draw
    bool _charging = false;

    static int _bucket(int level) { return level / 5; }

public:
    BatteryWidget(IDisplayController* gfx,
                  int16_t x, int16_t y,
                  uint16_t w = 100, uint16_t h = 25)
        : Widget(gfx, x, y, w, h) {}

    /**
     * @brief Feed updated battery state; marks dirty on 5 % bucket change or
     *        charging state flip.
     * @param level    Battery level 0–100 %.
     * @param charging True when a USB supply is detected.
     */
    void update(int level, bool charging) {
        if (_level < 0
         || _bucket(level) != _bucket(_level)
         || charging != _charging) {
            _level    = level;
            _charging = charging;
            markDirty();
        }
    }

    void draw() override {
        _gfx->fillRect(_x, _y, _w, _h, TFT_WHITE);

        int cellX = _x + _w - 44;
        int cellY = _y + 3;

        // Outer cell outline + positive terminal nub
        _gfx->drawRoundRect(cellX, cellY, 40, 20, 3, TFT_BLACK);
        _gfx->fillRect(cellX + 40, cellY + 5, 4, 10, TFT_BLACK);

        if (_charging) {
            // Bold '+' cross inside the cell
            _gfx->fillRect(cellX + 17, cellY + 6,  6,  8, TFT_BLACK); // vertical
            _gfx->fillRect(cellX + 14, cellY + 8, 12,  4, TFT_BLACK); // horizontal
        } else {
            int fillW = (36 * std::max(0, std::min(100, _level))) / 100;
            if (fillW > 0) {
                if (_level <= 15) {
                    // Striped fill for critical level warning
                    for (int col = 0; col < fillW; col++) {
                        if ((col / 2) % 2 == 0)
                            _gfx->fillRect(cellX + 2 + col, cellY + 2, 1, 16, TFT_BLACK);
                    }
                } else {
                    _gfx->fillRect(cellX + 2, cellY + 2, fillW, 16, TFT_BLACK);
                }
            }
        }

        // Percentage text left of the cell
        char buf[16];
        if (_charging)
            snprintf(buf, sizeof(buf), "+%d%%", _level);
        else if (_level <= 15)
            snprintf(buf, sizeof(buf), "%d%%!", _level);
        else
            snprintf(buf, sizeof(buf), "%d%%",  _level);

        _gfx->setFont(WidgetFont::Tiny);
        _gfx->setTextSize(1.0f);
        _gfx->setTextColor(TFT_BLACK);
        _gfx->setTextDatum(WidgetDatum::MiddleRight);
        _gfx->drawString(buf, cellX - 5, cellY + 10);
        _gfx->setTextDatum(WidgetDatum::TopLeft);

        clearDirty();
    }
};

#endif // BATTERY_WIDGET_H
