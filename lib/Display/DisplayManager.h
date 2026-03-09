/**
 * @file DisplayManager.h
 * @brief High-level drawing layer for the M5Paper 4.7" e-ink display.
 *
 * DisplayManager wraps M5GFX to provide all visual operations needed by the
 * weather application: provisioning QR screen, PIN entry pad, loading splash,
 * full weather UI, and fast partial-refresh forecast scrolling.
 *
 * The singleton pattern ensures that only one set of display state exists and
 * that concurrent tasks do not collide when updating the screen.
 */
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H
#include <Arduino.h>
#include <M5Unified.h>
#include <WeatherService.h>

/**
 * @enum Page
 * @brief Represents the three interactive full-screen views.
 */
enum class Page {
    Dashboard,
    Forecast,
    Settings
};

/**
 * @class DisplayManager
 * @brief Singleton that owns all rendering logic for the M5Paper e-ink screen.
 *
 * Internally distinguishes between full-screen refreshes (epd_quality) for
 * complete weather redraws and fast partial refreshes (epd_fastest) for
 * clock-tick and forecast-scroll updates that must not flash the whole display.
 */
class DisplayManager {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single DisplayManager object.
     */
    static DisplayManager& getInstance();

    /**
     * @brief Initialise the display driver.
     *
     * Must be called after M5.begin() and before any drawing methods.
     */
    void begin();

    /**
     * @brief Render the full-screen Wi-Fi provisioning view.
     *
     * Draws a large QR code encoding @p apUrl in the centre of the screen,
     * with the AP SSID and URL printed as text below it.
     *
     * @param ssid   The SoftAP network name shown on screen.
     * @param apUrl  The URL encoded into the QR code (typically http://192.168.4.1).
     */
    void showProvisioningScreen(const String& ssid, const String& apUrl);

    /**
     * @brief Show an interactive numeric PIN-entry pad on the touchscreen.
     *
     * Blocks until the user taps Confirm or Cancel.
     *
     * @param message  Short prompt displayed above the keypad.
     * @return         The digit string entered by the user, or @c "" if cancelled.
     */
    String promptPIN(const String& message = "Enter PIN");

    /**
     * @brief Fill the screen with white.
     *
     * Use before any full-screen redraw to avoid ghosting.
     */
    void clear();

    /**
     * @brief Render the currently active page.
     *
     * Dispatches rendering logic to the dedicated page drawing routines depending on _activePage.
     *
     * @param data            Latest WeatherData from WeatherService.
     * @param localTime       Current local time (used for clock display).
     * @param city            Location string shown at the top (e.g. "Peoria, IL").
     * @param fastMode        If @c true, uses epd_fastest (partial refresh) for
     *                        clock-only ticks.  If @c false, uses epd_quality
     *                        for a full data refresh.
     * @param forecastOffset  Index into the 10-day array for the leftmost column.
     * @param settingsCursor  Index of the currently highlighted menu item on the Settings page.
     */
    void renderActivePage(const WeatherData& data, const struct tm& localTime,
                          const String& city, bool fastMode = false, int forecastOffset = 0, int settingsCursor = 0);

    Page getActivePage() const { return _activePage; }
    void setActivePage(Page p) { _activePage = p; }

    // Page-specific renderers
    void drawPageDashboard(const WeatherData& data, const struct tm& localTime, const String& city);
    void drawPageForecast(const WeatherData& data, int forecastOffset);
    void drawPageSettings(int settingsCursor);

    /**
     * @brief Partially refresh only the forecast strip at the bottom of the screen.
     *
     * Used during horizontal swipe scrolling so the upper weather section is
     * not reflashed unnecessarily.
     *
     * @param data            Latest WeatherData.
     * @param forecastOffset  First day index to display in the 3-column strip.
     */
    void updateForecastUI(const WeatherData& data, int forecastOffset = 0);

    /**
     * @brief Display a loading splash screen while the first weather fetch runs.
     * @param city  Location string shown on the splash (e.g. "Peoria, IL").
     */
    void showLoadingScreen(const String& city);

    /**
     * @brief Render a simple full-screen informational message.
     *
     * Useful for error states (e.g. "No Wi-Fi", "API Error").
     *
     * @param title  Bold heading text.
     * @param body   Detail text shown below the heading.
     */
    void showMessage(const String& title, const String& body);

private:
    DisplayManager() = default;

    M5Canvas _canvas{(&M5.Display)};
    Page _activePage = Page::Dashboard;

    /**
     * @brief Draw QR code modules at an arbitrary screen position.
     * @param url         Content to encode.
     * @param ox          X pixel offset of the top-left module.
     * @param oy          Y pixel offset of the top-left module.
     * @param moduleSize  Pixel size of each QR module.
     */
    void _drawQR(const String& url, int ox, int oy, int moduleSize);

    /**
     * @struct Rect
     * @brief Axis-aligned bounding rectangle (pixels).
     */
    struct Rect { int x, y, w, h; };

    /**
     * @brief Draw a filled rectangular soft-button with centred label.
     * @param label  Text drawn inside the button.
     * @param x      Left edge (pixels).
     * @param y      Top edge (pixels).
     * @param w      Width (pixels).
     * @param h      Height (pixels).
     * @param bg     Background fill colour (RGB888).
     * @param fg     Foreground (text) colour (RGB888).
     * @return       Bounding Rect of the drawn button.
     */
    Rect _drawButton(const String& label, int x, int y, int w, int h,
                     uint32_t bg, uint32_t fg);

    /**
     * @brief Render the battery gauge in the top-right corner.
     * Evaluates M5.Power.getBatteryLevel() and draws a dynamic cell.
     */
    void _drawBattery();

    /**
     * @brief Draw a visual dial indicating current Air Quality Index.
     * @param aqi  Current US AQI.
     * @param yOff Vertical offset on screen.
     */
    void _drawAQIGauge(int aqi, int yOff);

    /**
     * @brief Draw an astronomical sun arc dial indicating time until sunset/sunrise.
     * @param current Unix timestamp for now.
     * @param sunrise Unix timestamp for today's sunrise.
     * @param sunset  Unix timestamp for today's sunset.
     * @param yOff    Vertical offset on screen.
     */
    void _drawSunArc(time_t current, time_t sunrise, time_t sunset, int yOff);
    
    // Phase 5 Additions
    void _drawWeatherIcon(const char* condition, int x, int y, int size);
    void _drawForecastSparkline(const WeatherData& data, int yOff);
    void _drawPagination(int totalPages, int currentPage);
    void _drawLastUpdated(time_t fetchTime);

    float         _cachedBatVoltage = 0.0f;
    int           _cachedBatLevel = 0;
    uint32_t      _lastBatUpdateMs = 0;

    static constexpr int kWidth  = 540; ///< Display width in pixels.
    static constexpr int kHeight = 960; ///< Display height in pixels.
};

#endif // DISPLAY_MANAGER_H
