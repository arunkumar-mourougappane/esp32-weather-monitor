#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <Arduino.h>
#include <WeatherService.h>

/**
 * @class AppController
 * @brief Manages the main application lifecycle, including Deep Sleep routines.
 *
 * Implements an event-driven awake/sleep cycle:
 * 1. Timer Wakeup (Every 30 mins) -> Fetches weather, updates display, sleeps.
 * 2. Button Wakeup (G38 Click) -> Stays awake for 30 seconds for user interaction (swipes/scrolls).
 */
class AppController {
public:
    static AppController& getInstance();

    /**
     * @brief Initiates the state machine based on the ESP_SLEEP_WAKEUP cause.
     */
    void begin();

    /**
     * @brief Puts the device into Deep Sleep.
     */
    void enterDeepSleep();

private:
    AppController() = default;

    /**
     * @brief The interactive loop that runs when the user manually wakes the device.
     */
    void _runInteractiveSession(const String& locationStr);

    /**
     * @brief Sleep with a 1-second timer so the device wakes immediately
     *        and passes through the normal timer-wakeup fetch path.
     *
     * Used by "Force Sync Now" to trigger an on-demand weather refresh
     * without bypassing the established WiFi → fetch → render sequence.
     */
    void _enterDeepSleepForImmediateWakeup();

    /**
     * @brief Run the Always-On Minimal display mode state machine.
     *
     * On timer wakeup, either performs a full WiFi sync (when
     * rtcMinutesSinceSync >= always_on_sync_interval) or a fast clock-only
     * partial refresh, then sleeps until the next minute boundary.
     * On button (EXT0) wakeup, falls through to the standard interactive session.
     */
    void _runMinimalAlwaysOnMode();

    /**
     * @brief Enter deep sleep until the top of the next minute.
     *
     * Used exclusively by the minimal always-on mode to produce 1-minute
     * timer-tick wakeups that land as close as possible to HH:MM:00.
     */
    void _enterMinimalDeepSleep();
};

#endif // APP_CONTROLLER_H
