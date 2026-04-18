/**
 * @file IDisplayController.h
 * @brief Hardware-agnostic drawing interface for the Widget / Page engine.
 *
 * IDisplayController abstracts all drawing primitives so that Widget subclasses
 * never include M5GFX headers directly.  The concrete M5CanvasController
 * implementation (M5CanvasController.h) maps every call to the appropriate
 * LGFX / M5Canvas API.
 *
 * ThemeColor provides semantic colour tokens so widgets and pages can express
 * intent (foreground, background, highlight, warning) independently of the
 * physical display technology.  Each concrete driver maps tokens to its own
 * native colour constants — e.g. EInkDriver maps every non-background token
 * to black, while a hypothetical TFTDriver could map Highlight to orange.
 */
#ifndef IDISPLAY_CONTROLLER_H
#define IDISPLAY_CONTROLLER_H

#include <stdint.h>

// ── Semantic colour tokens ────────────────────────────────────────────────────
/**
 * @enum ThemeColor
 * @brief Display-technology-agnostic colour identifiers.
 *
 * Widgets use these tokens instead of raw TFT_* constants so that the same
 * widget code renders correctly on both e-ink (monochrome) and future colour
 * displays.  Concrete IDisplayController implementations map each token to
 * the appropriate native colour via resolveColor().
 */
enum class ThemeColor {
    Background, ///< Primary background  (e-ink: white)
    Foreground, ///< Primary text/lines  (e-ink: black)
    Highlight,  ///< Accent / selection  (e-ink: black; TFT: orange)
    Warning,    ///< Alert / error       (e-ink: black; TFT: red)
};

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

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    /** @brief Initialise the display hardware (rotation, colour depth, etc.). */
    virtual void init() = 0;

    /**
     * @brief Fill the entire canvas with @p color and push to the display.
     * @param color  Semantic colour token; defaults to Background.
     */
    virtual void clear(ThemeColor color = ThemeColor::Background) = 0;

    /**
     * @brief Push the full canvas to the display using a high-quality refresh.
     *
     * Concrete implementations should use the highest-quality update mode
     * (e.g. epd_quality on e-ink) to ensure a clean, ghost-free full render.
     */
    virtual void flushFull() = 0;

    /** @brief Put the display into low-power sleep mode. */
    virtual void sleep() = 0;

    /**
     * @brief Map a ThemeColor token to the native colour constant for this driver.
     * @param c  Semantic colour token.
     * @return   Native colour value (e.g. TFT_BLACK / TFT_WHITE on e-ink).
     */
    virtual uint32_t resolveColor(ThemeColor c) const = 0;

    // ── Shapes ────────────────────────────────────────────────────────────────
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

    // ── Text ──────────────────────────────────────────────────────────────────
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
