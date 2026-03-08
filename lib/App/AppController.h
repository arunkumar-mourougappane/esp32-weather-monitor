/**
 * @file AppController.h
 * @brief Top-level application orchestrator for the M5Paper Weather Monitor.
 *
 * AppController is a singleton that owns and manages three FreeRTOS tasks:
 *  - A weather-fetch task that periodically contacts the Google Weather API.
 *  - A display task that redraws the e-ink screen on new data or minute ticks.
 *  - An NTP task that periodically re-synchronises system time.
 *
 * @note Call begin() exactly once from setup(), after WiFi and NTP are ready.
 */
#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <WeatherService.h>

/**
 * @class AppController
 * @brief Singleton orchestrator that spawns and coordinates all FreeRTOS tasks.
 *
 * Typical lifecycle:
 * @code
 *   AppController::getInstance().begin();
 * @endcode
 */
class AppController {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single AppController object.
     */
    static AppController& getInstance();

    /**
     * @brief Kick off all application tasks.
     *
     * Creates the weather-fetch, display, and NTP FreeRTOS tasks. Should be
     * called once from setup() after Wi-Fi and initial NTP sync are complete.
     */
    void begin();

    /**
     * @brief Thread-safe snapshot of the most recently fetched weather data.
     * @return A copy of the latest WeatherData struct (protected by mutex).
     */
    WeatherData getWeather();

    /**
     * @brief Signal the display task to refresh immediately.
     *
     * Sets the internal dirty flag so that the display task redraws the UI
     * on its next iteration without waiting for the normal interval.
     */
    void requestDisplayUpdate();

private:
    AppController() = default;

    // ── Task entry-points ────────────────────────────────────────────────────
    /** @brief FreeRTOS task that periodically fetches weather data. */
    static void _weatherTaskFn(void* param);
    /** @brief FreeRTOS task that drives display refreshes and input polling. */
    static void _displayTaskFn(void* param);
    /** @brief FreeRTOS task that periodically re-syncs NTP time. */
    static void _ntpTaskFn(void*    param);

    // ── Task handles ─────────────────────────────────────────────────────────
    TaskHandle_t _weatherTask = nullptr; ///< Handle for the weather-fetch task.
    TaskHandle_t _displayTask = nullptr; ///< Handle for the display-update task.
    TaskHandle_t _ntpTask     = nullptr; ///< Handle for the NTP re-sync task.

    // ── Shared state ─────────────────────────────────────────────────────────
    WeatherData       _weather;                  ///< Latest weather snapshot.
    SemaphoreHandle_t _weatherMutex = nullptr;   ///< Guards access to _weather.

    volatile bool _displayDirty  = true;  ///< True when display needs a redraw.
    int           _forecastOffset= 0;     ///< Current horizontal scroll offset.
};

#endif // APP_CONTROLLER_H
