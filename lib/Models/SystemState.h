#ifndef MODELS_SYSTEM_STATE_H
#define MODELS_SYSTEM_STATE_H

/**
 * @file lib/Models/SystemState.h
 * @brief Single RTC-persistent state struct for the ESP32 weather monitor.
 *
 * All state that must survive deep sleep is consolidated here into one
 * RTC_DATA_ATTR instance (g_state), declared in src/main.cpp.
 *
 * Consolidation goals:
 *  - Single point of truth for every persisted variable.
 *  - No RTC globals scattered across AppController.cpp / WiFiManager.cpp.
 *  - Modules receive a reference to g_state at begin() time; they never
 *    own their own RTC storage.
 *
 * References: docs/research/application-modularization-research.md §5
 */

#include <WeatherService.h>
#include <stdint.h>

struct SystemState {
    // ── Weather & display ─────────────────────────────────────────────────────
    WeatherData currentWeather;     ///< Last successfully fetched weather payload.
    int         forecastOffset;     ///< First visible column on the Forecast page.
    uint32_t    wakeupCount;        ///< Total timer/power-on wakeups since factory reset.
    uint8_t     activePage;         ///< Active PageId cast to uint8_t (survives deep sleep).
    int         settingsCursor;     ///< Highlighted row on the Settings page.
    char        lastIP[16];         ///< Dotted-decimal IP from the last successful WiFi connect.
    uint8_t     lastError;          ///< AppError code from the most recent fetch cycle.
    uint8_t     ghostCount;         ///< Full-quality redraw counter for ghost cleanup.

    // ── Barometric pressure ring (trend computation) ──────────────────────────
    float       pressureRing[3];    ///< Rolling surface-pressure readings (hPa).
    uint8_t     pressureCount;      ///< Valid entries in pressureRing (clamped to 3).

    // ── Battery discharge ring (runtime estimation) ───────────────────────────
    int32_t     batRing[8];         ///< Rolling battery voltage samples (mV).
    uint8_t     batRingHead;        ///< Next write index (ring-buffer head).
    uint8_t     batRingCount;       ///< Valid samples in batRing (clamped to 8).

    // ── Minimal Always-On mode ────────────────────────────────────────────────
    /**
     * Minutes elapsed since the last weather sync.
     * Initialised to 30 on cold boot to force an immediate sync.
     */
    uint8_t     minutesSinceSync;

    // ── WiFiManager fast-connect cache ────────────────────────────────────────
    uint8_t     wifiBssid[6];       ///< BSSID of the last successfully connected AP.
    int32_t     wifiChannel;        ///< Channel of the last successfully connected AP (0 = uncached).
    char        wifiCachedSsid[33]; ///< SSID matching the cached BSSID/channel.
};

/**
 * @brief Single RTC-persistent application state.
 *
 * Defined once in src/main.cpp as:
 * @code
 *   RTC_DATA_ATTR SystemState g_state = {};
 * @endcode
 *
 * All modules that need RTC persistence should access members of this
 * struct rather than declaring their own RTC_DATA_ATTR variables.
 */
extern RTC_DATA_ATTR SystemState g_state;

#endif // MODELS_SYSTEM_STATE_H
