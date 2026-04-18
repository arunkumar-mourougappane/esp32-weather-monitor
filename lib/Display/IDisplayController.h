/**
 * @file IDisplayController.h
 * @brief Hardware-agnostic drawing interface for the Widget / Page engine.
 *
 * IDisplayController abstracts all drawing primitives so that Widget subclasses
 * never include M5GFX headers directly.  The concrete M5CanvasController
 * implementation (M5CanvasController.h) maps every call to the appropriate
 * LGFX / M5Canvas API.
 */
#ifndef IDISPLAY_CONTROLLER_H
#define IDISPLAY_CONTROLLER_H

#include <stdint.h>

// ── Font selector ─────────────────────────────────────────────────────────────
enum class WidgetFont {
    Tiny,        // nullptr  — system default raster font
    Small,       // FreeSans9pt7b
    SmallBold,   // FreeSansBold9pt7b
    Medium,      // FreeSans12pt7b
    Large,       // FreeSans18pt7b
    LargeBold,   // FreeSansBold18pt7b
    XLarge,      // FreeSans24pt7b
    XLargeBold,  // FreeSansBold24pt7b
};

// ── Text alignment ────────────────────────────────────────────────────────────
enum class WidgetDatum {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
};

// ── Interface ─────────────────────────────────────────────────────────────────
class IDisplayController {
public:
    virtual ~IDisplayController() = default;

    // Shapes
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) = 0;
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) = 0;
    virtual void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint32_t color) = 0;
    virtual void fillCircle(int16_t x, int16_t y, int16_t r, uint32_t color) = 0;
    virtual void drawCircle(int16_t x, int16_t y, int16_t r, uint32_t color) = 0;
    virtual void drawArc(int16_t x, int16_t y, int16_t r0, int16_t r1,
                         float a0, float a1, uint32_t color) = 0;
    virtual void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              int16_t x2, int16_t y2, uint32_t color) = 0;
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint32_t color) = 0;
    virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint32_t color) = 0;
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color) = 0;
    virtual void drawPixel(int16_t x, int16_t y, uint32_t color) = 0;

    // Text
    virtual void setFont(WidgetFont font) = 0;
    virtual void setTextSize(float size) = 0;
    virtual void setTextColor(uint32_t color) = 0;
    virtual void setTextDatum(WidgetDatum datum) = 0;
    virtual void drawString(const char* text, int16_t x, int16_t y) = 0;
    virtual void drawCentreString(const char* text, int16_t x, int16_t y) = 0;

    // Weather icon (condition string → nearest-neighbour scaled XBM bitmap)
    virtual void drawWeatherIcon(const char* condition, int16_t x, int16_t y, int16_t size) = 0;

    // E-ink partial refresh: push the bounding box region to the physical panel.
    // Concrete implementations should use epd_fastest mode and limit the push to
    // the supplied rectangle to avoid a full-panel flash on every widget update.
    virtual void flushBoundingBox(int16_t x, int16_t y, int16_t w, int16_t h) = 0;

    // Display dimensions (needed by PageBase for full-screen flush)
    virtual int16_t getWidth()  const = 0;
    virtual int16_t getHeight() const = 0;
};

#endif // IDISPLAY_CONTROLLER_H
