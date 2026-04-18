/**
 * @file WeatherIconHelper.h
 * @brief Shared weather-icon drawing function for DisplayManager and Widget impls.
 *
 * Extracted from DisplayManager::_drawWeatherIcon so that both the legacy
 * rendering path and the new Widget / M5CanvasController path share one
 * authoritative implementation, preventing the two copies from diverging.
 *
 * The template accepts any M5GFX canvas (M5Canvas, LGFX_Sprite) since the
 * only canvas primitive used is drawPixel(), which is defined on LGFXBase.
 */
#ifndef WEATHER_ICON_HELPER_H
#define WEATHER_ICON_HELPER_H

#include "weather_bitmaps.h"
#include <Arduino.h>

/**
 * @brief Draw a weather-condition icon onto any LGFX-compatible canvas.
 *
 * Selects the matching XBM bitmap from weather_bitmaps.h using the same
 * condition-string rules as DisplayManager::_drawWeatherIcon, then scales
 * the 32×32 source image to @p size × @p size pixels using nearest-neighbour
 * scaling, centred on (@p x, @p y).
 *
 * @tparam Canvas  Any LGFX canvas type that provides drawPixel(x, y, color).
 * @param canvas    Target canvas.
 * @param condition Weather condition string (e.g. "Partly Cloudy").
 * @param x         Centre X of the icon.
 * @param y         Centre Y of the icon.
 * @param size      Pixel side-length of the rendered icon.
 */
template <typename Canvas>
void drawWeatherIconOnCanvas(Canvas& canvas, const char* condition, int x, int y, int size) {
    String cond = condition;
    cond.toLowerCase();

    const uint8_t* bmp = icon_cloudy_bmp; // default fallback

    if      (cond.indexOf("tornado")       >= 0)                                         bmp = icon_tornado_bmp;
    else if (cond.indexOf("hurricane")     >= 0 || cond.indexOf("tropical storm") >= 0)  bmp = icon_hurricane_bmp;
    else if (cond.indexOf("hail")          >= 0)                                         bmp = icon_hail_bmp;
    else if (cond.indexOf("thunder")       >= 0 || cond.indexOf("t-storm") >= 0
             || cond.indexOf("tstorm")     >= 0)                                         bmp = icon_thunder_bmp;
    else if (cond.indexOf("freezing")      >= 0)                                         bmp = icon_freezing_rain_bmp;
    else if (cond.indexOf("sleet")         >= 0 || cond.indexOf("mixed")   >= 0)         bmp = icon_sleet_bmp;
    else if (cond.indexOf("blowing snow")  >= 0 || cond.indexOf("blizzard") >= 0)        bmp = icon_blowing_snow_bmp;
    else if (cond.indexOf("heavy snow")    >= 0)                                         bmp = icon_heavy_snow_bmp;
    else if (cond.indexOf("snow")          >= 0 || cond.indexOf("flurr")   >= 0)         bmp = icon_snow_bmp;
    else if (cond.indexOf("heavy shower")  >= 0 || cond.indexOf("heavy rain") >= 0)      bmp = icon_heavy_showers_bmp;
    else if (cond.indexOf("drizzle")       >= 0)                                         bmp = icon_drizzle_bmp;
    else if (cond.indexOf("rain")          >= 0 || cond.indexOf("shower")  >= 0)         bmp = icon_rain_bmp;
    else if (cond.indexOf("fog")           >= 0)                                         bmp = icon_foggy_bmp;
    else if (cond.indexOf("haze")          >= 0 || cond.indexOf("mist")    >= 0)         bmp = icon_haze_bmp;
    else if (cond.indexOf("smoke")         >= 0 || cond.indexOf("ash")     >= 0)         bmp = icon_smoky_bmp;
    else if (cond.indexOf("dust")          >= 0 || cond.indexOf("sand")    >= 0)         bmp = icon_haze_bmp;
    else if (cond.indexOf("blustery")      >= 0 || cond.indexOf("squall")  >= 0)         bmp = icon_blustery_bmp;
    else if (cond.indexOf("wind")          >= 0 || cond.indexOf("breezy")  >= 0)         bmp = icon_windy_bmp;
    else if (cond.indexOf("mostly cloudy") >= 0 || cond.indexOf("overcast") >= 0)        bmp = icon_mostly_cloudy_bmp;
    else if (cond.indexOf("partly cloudy") >= 0 || cond.indexOf("partly sunny") >= 0)    bmp = icon_partly_cloudy_bmp;
    else if (cond.indexOf("cloudy")        >= 0)                                         bmp = icon_cloudy_bmp;
    else if (cond.indexOf("sun")           >= 0 || cond.indexOf("clear") >= 0
             || cond.indexOf("fair")       >= 0)                                         bmp = icon_clear_bmp;
    else if (cond.indexOf("n/a")           >= 0 || cond.indexOf("unknown") >= 0
             || cond.length()             == 0)                                          bmp = icon_not_available_bmp;

    static constexpr int SRC_W = 32, SRC_H = 32, BYTES_PER_ROW = 4;
    int draw_x = x - size / 2;
    int draw_y = y - size / 2;
    for (int oy = 0; oy < size; oy++) {
        int sy = oy * SRC_H / size;
        for (int ox = 0; ox < size; ox++) {
            int sx    = ox * SRC_W / size;
            uint8_t b = pgm_read_byte(&bmp[sy * BYTES_PER_ROW + sx / 8]);
            bool dark = (b >> (sx % 8)) & 1;
            canvas.drawPixel(draw_x + ox, draw_y + oy, dark ? TFT_BLACK : TFT_WHITE);
        }
    }
}

#endif // WEATHER_ICON_HELPER_H
