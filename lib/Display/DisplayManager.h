#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <WeatherService.h>

/// Wraps M5GFX to provide higher-level drawing operations for the M5Paper
/// e-ink display: QR code, provisioning screen, PIN pad, and weather UI.
class DisplayManager {
public:
    static DisplayManager& getInstance();

    /// Must be called after M5.begin().
    void begin();

    /// Draw a full-screen provisioning screen:
    ///   - Large QR code encoding @p apUrl
    ///   - Caption lines showing SSID and URL below the QR
    void showProvisioningScreen(const String& ssid, const String& apUrl);

    /// Draw a numeric PIN entry pad and collect input from the touch panel.
    /// @param message  Optional prompt shown above the pad.
    /// @return         The digit string the user confirmed, or "" if cancelled.
    String promptPIN(const String& message = "Enter PIN");

    /// Fill screen white (use before any full-screen redraw).
    void clear();

    /// Render the weather summary on screen.
    /// @param fastMode  If true, uses epd_fastest for clock-only refresh.
    /// @param fastMode  If true, uses epd_fastest for clock-only refresh.
    ///                  If false, uses epd_quality for full weather update.
    /// @param forecastOffset Index to start drawing the 3-day forecast window.
    void showWeatherUI(const WeatherData& data, const struct tm& localTime,
                       const String& city, bool fastMode = false, int forecastOffset = 0);

    /// Partially update ONLY the bottom forecast section of the display
    /// (useful for fast scrolling without flashing the whole screen).
    void updateForecastUI(const WeatherData& data, int forecastOffset = 0);

    /// Shown immediately on boot while weather is still being fetched.
    void showLoadingScreen(const String& city);

    /// Show a simple full-screen status/error message.
    void showMessage(const String& title, const String& body);

private:
    DisplayManager() = default;

    /// Internal: render QR code modules at position (ox, oy) with given pixel size.
    void _drawQR(const String& url, int ox, int oy, int moduleSize);

    /// Internal: draw a rectangular button and return its screen rect.
    struct Rect { int x, y, w, h; };
    Rect _drawButton(const String& label, int x, int y, int w, int h,
                     uint32_t bg, uint32_t fg);

    static constexpr int kWidth  = 540;
    static constexpr int kHeight = 960;
};
