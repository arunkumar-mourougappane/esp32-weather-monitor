/**
 * @file M5CanvasController.h
 * @brief Concrete IDisplayController that delegates drawing to an M5Canvas.
 *
 * M5CanvasController wraps a reference to an existing M5Canvas (LGFX_Sprite)
 * and maps:
 *   - WidgetFont enum  → LGFX font pointers (FreeSans / FreeSansBold family)
 *   - WidgetDatum enum → LGFX text-datum constants (TL_DATUM, TC_DATUM …)
 *   - drawWeatherIcon  → drawWeatherIconOnCanvas<M5Canvas> (WeatherIconHelper.h)
 *   - flushBoundingBox → epd_fastest partial refresh of the widget's rectangle
 *
 * flushBoundingBox() uses M5.Display.setClipRect() to restrict the canvas
 * push to the widget's bounding box, so the e-ink panel only cycles the pixels
 * that actually changed.  The full-screen canvas (_canvas) is always the single
 * source of truth; no per-widget sub-sprites are needed for the basic case.
 */
#ifndef M5_CANVAS_CONTROLLER_H
#define M5_CANVAS_CONTROLLER_H

#include "IDisplayController.h"
#include "WeatherIconHelper.h"
#include <M5Unified.h>

class M5CanvasController : public IDisplayController {
    M5Canvas& _canvas;

    static const lgfx::IFont* _toFont(WidgetFont f) {
        switch (f) {
            case WidgetFont::Small:      return &fonts::FreeSans9pt7b;
            case WidgetFont::SmallBold:  return &fonts::FreeSansBold9pt7b;
            case WidgetFont::Medium:     return &fonts::FreeSans12pt7b;
            case WidgetFont::Large:      return &fonts::FreeSans18pt7b;
            case WidgetFont::LargeBold:  return &fonts::FreeSansBold18pt7b;
            case WidgetFont::XLarge:     return &fonts::FreeSans24pt7b;
            case WidgetFont::XLargeBold: return &fonts::FreeSansBold24pt7b;
            default:                     return nullptr; // Tiny → system raster font
        }
    }

    static uint8_t _toDatum(WidgetDatum d) {
        switch (d) {
            case WidgetDatum::TopLeft:      return TL_DATUM;
            case WidgetDatum::TopCenter:    return TC_DATUM;
            case WidgetDatum::TopRight:     return TR_DATUM;
            case WidgetDatum::MiddleLeft:   return ML_DATUM;
            case WidgetDatum::MiddleCenter: return MC_DATUM;
            case WidgetDatum::MiddleRight:  return MR_DATUM;
            default:                        return TL_DATUM;
        }
    }

public:
    explicit M5CanvasController(M5Canvas& canvas) : _canvas(canvas) {}

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void init() override {
        M5.Display.setRotation(0);
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        M5.Display.fillScreen(TFT_WHITE);
        _canvas.fillSprite(TFT_WHITE);
    }

    void clear(ThemeColor color = ThemeColor::Background) override {
        uint32_t c = resolveColor(color);
        _canvas.fillSprite(c);
        M5.Display.fillScreen(c);
    }

    void flushFull() override {
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        _canvas.pushSprite(0, 0);
    }

    void sleep() override {
        M5.Display.sleep();
    }

    uint32_t resolveColor(ThemeColor c) const override {
        switch (c) {
            case ThemeColor::Background: return TFT_WHITE;
            case ThemeColor::Foreground: return TFT_BLACK;
            case ThemeColor::Highlight:  return TFT_BLACK; // e-ink: no colour distinction
            case ThemeColor::Warning:    return TFT_BLACK; // e-ink: no colour distinction
            default:                     return TFT_BLACK;
        }
    }

    // ── Shapes ────────────────────────────────────────────────────────────────
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c) override
        { _canvas.fillRect(x, y, w, h, c); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c) override
        { _canvas.drawRect(x, y, w, h, c); }
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint32_t c) override
        { _canvas.drawRoundRect(x, y, w, h, r, c); }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint32_t c) override
        { _canvas.fillCircle(x, y, r, c); }
    void drawCircle(int16_t x, int16_t y, int16_t r, uint32_t c) override
        { _canvas.drawCircle(x, y, r, c); }
    void drawArc(int16_t x, int16_t y, int16_t r0, int16_t r1,
                 float a0, float a1, uint32_t c) override
        { _canvas.drawArc(x, y, r0, r1, a0, a1, c); }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint32_t c) override
        { _canvas.fillTriangle(x0, y0, x1, y1, x2, y2, c); }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint32_t c) override
        { _canvas.drawFastHLine(x, y, w, c); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint32_t c) override
        { _canvas.drawFastVLine(x, y, h, c); }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t c) override
        { _canvas.drawLine(x0, y0, x1, y1, c); }
    void drawPixel(int16_t x, int16_t y, uint32_t c) override
        { _canvas.drawPixel(x, y, c); }

    // ── Text ──────────────────────────────────────────────────────────────────
    void setFont(WidgetFont f) override     { _canvas.setFont(_toFont(f)); }
    void setTextSize(float s) override      { _canvas.setTextSize(s); }
    void setTextColor(uint32_t c) override  { _canvas.setTextColor(c); }
    void setTextDatum(WidgetDatum d) override { _canvas.setTextDatum(_toDatum(d)); }
    void drawString(const char* t, int16_t x, int16_t y) override
        { _canvas.drawString(t, x, y); }
    void drawCentreString(const char* t, int16_t x, int16_t y) override
        { _canvas.drawCentreString(t, x, y); }

    // ── Weather icon ──────────────────────────────────────────────────────────
    void drawWeatherIcon(const char* cond, int16_t x, int16_t y, int16_t size) override
        { drawWeatherIconOnCanvas(_canvas, cond, x, y, size); }

    // ── E-ink partial refresh ─────────────────────────────────────────────────
    // Sets a clip rect on M5.Display before pushing the full canvas so that the
    // e-ink driver only cycles the pixels inside the widget's bounding box.
    void flushBoundingBox(int16_t x, int16_t y, int16_t w, int16_t h) override {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        M5.Display.startWrite();
        M5.Display.setClipRect(x, y, w, h);
        _canvas.pushSprite(0, 0);
        M5.Display.clearClipRect();
        M5.Display.endWrite();
    }

    int16_t getWidth()  const override { return static_cast<int16_t>(_canvas.width()); }
    int16_t getHeight() const override { return static_cast<int16_t>(_canvas.height()); }
};

#endif // M5_CANVAS_CONTROLLER_H
