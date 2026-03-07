#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../network/WeatherService.h"

/// Spawns and manages FreeRTOS tasks for normal (post-provisioning) operation.
class AppController {
public:
    static AppController& getInstance();

    /// Kick off all application tasks.  Call once from setup() after
    /// WiFi + NTP are ready.
    void begin();

    /// Thread-safe copy of the latest weather data.
    WeatherData getWeather();

    /// Signal the display task to refresh immediately.
    void requestDisplayUpdate();

private:
    AppController() = default;

    // ── Task functions ────────────────────────────────────────────────────────
    static void _weatherTaskFn(void* param);
    static void _displayTaskFn(void* param);
    static void _ntpTaskFn(void*    param);

    // ── Task handles ─────────────────────────────────────────────────────────
    TaskHandle_t _weatherTask = nullptr;
    TaskHandle_t _displayTask = nullptr;
    TaskHandle_t _ntpTask     = nullptr;

    // ── Shared state ─────────────────────────────────────────────────────────
    WeatherData       _weather;
    SemaphoreHandle_t _weatherMutex = nullptr;

    // Display update notification flag
    volatile bool _displayDirty = true;
};
