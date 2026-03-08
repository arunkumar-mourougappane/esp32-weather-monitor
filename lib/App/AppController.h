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
    void _runInteractiveSession();
};

#endif // APP_CONTROLLER_H
