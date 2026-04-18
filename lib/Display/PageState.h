#ifndef PAGE_STATE_H
#define PAGE_STATE_H

#include <Arduino.h>
#include <WeatherService.h>
#include <time.h>

/**
 * @struct PageState
 * @brief Aggregates all runtime state required by IPage implementations.
 *
 * Passed to IPage::updateData() so pages can access weather, time, location,
 * and UI cursor state without coupling to AppController or RTC variables.
 *
 * @note For RTC-persistent state that survives deep sleep, see
 *       lib/Models/SystemState.h and the g_state global.
 */
struct PageState {
    WeatherData weather;        ///< Latest weather payload (check weather.valid).
    struct tm   localTime;      ///< Current local time.
    String      city;           ///< Display location string (e.g. "Peoria, IL").
    int         forecastOffset; ///< First visible column index on the Forecast page.
    int         settingsCursor; ///< Highlighted item index on the Settings page.
    bool        ntpFailed;      ///< True when last NTP sync failed (RTC fallback).
    bool        overlayActive;  ///< True when the hourly overlay is shown.
};

#endif // PAGE_STATE_H
