#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "../network/WeatherService.h"

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

    /// Placeholder weather UI — renders data summary on screen.
    void showWeatherUI(const WeatherData& data, const struct tm& localTime,
                       const String& city);

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
