# Changelog

All notable changes to the `esp32-weather-monitor` project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added

* **AES-256-CTR NVS credential encryption** — All sensitive values written to non-volatile storage (`wifi_pass_N`, `api_key`, `webhook_url`) are now encrypted with AES-256-CTR before being persisted. The 32-byte key is derived from the device-unique eFuse MAC address via SHA-256, making ciphertext hardware-bound. A random 16-byte IV (from the ESP32 hardware TRNG) is generated on every write; the stored format is `"E1:" + Base64(IV || ciphertext)`. Decryption transparently handles legacy plaintext values (no `"E1:"` prefix) so existing NVS is automatically upgraded on first boot — no re-provisioning required. The upgrade is gated by a `cfg_encv` NVS flag and runs once in `begin()`.
* **Multi-Network WiFi with RSSI-based selection** — Up to 5 SSID/password pairs can be stored in NVS (`w_ssid_0`..`w_ssid_4` / `w_pass_0`..`w_pass_4` + `w_count`). `WiFiManager::connectBestSTA()` performs an active scan, ranks matching SSIDs by RSSI, and connects to the strongest available network. The fast-connect cache (`rtc_cached_ssid` + `rtc_bssid` + `rtc_channel`) is now keyed to the last connected SSID so it is reused only when that same SSID is in the candidate list. Falls back to config order when scanning fails or returns no matches.
* **Provisioning portal — multi-WiFi UI** — The WiFi fieldset is replaced with a dynamic list. Users can add up to 5 networks with **+ Add Network** or remove extras with **× Remove**. Index 0 is marked as primary. The `ProvisionSaveCallback` type is simplified to `std::function<void(const WeatherConfig&)>`; the web server builds and validates the complete config before invoking it.
* **NVS migration** — On first run after upgrade, the legacy `wifi_ssid` / `wifi_pass` keys are read into slot 0 transparently. New saves write indexed keys and keep the legacy keys for rollback compatibility.
* **Moon Phase Widget** — A new widget on the Dashboard showing fractional moon phases updated constantly via unixtime logic.
* **Wind Rose Compass** — Displays prevailing wind direction as an 8-point Cartesian compass dial substituting raw text.
* **Configurable Sync Interval** — Web portal update enabling runtime alteration of sleep bounds bypassing the hardcoded 30m gap.
* **Battery-Adaptive Sync Rate** — Automatic power governance multiplying sync interval safely when voltage < 40% (3650 mV).
* **Hourly Forecast Strip** — Expanding the Open-Meteo hook with granular array of 24h predictions inside a newly accessible 'Hourly' page.
* **Swipe-Up Detail Overlay** — Additional vertical metrics overlay spanning across page domains invoked by sliding fingers up the screen.
* **Double-Click Webhook** — Push alert proxy mechanism routing user-fed physical twin taps over to pre-allocated endpoints.
* **Quality-of-life UX improvements (items 2–15)**:
  * Dashboard detail labels expanded to full words: `Humidity`, `UV Index`, `Visibility`.
  * Wind speed now prefixed with 8-point compass direction (e.g. `NW 15 km/h`).
  * Tomorrow preview text shifted below icon bottom (Y 800 / 835) to eliminate overlap.
  * `showMessage()` font corrected — uses `FreeSansBold18pt7b` / `FreeSans12pt7b` instead of tiny bitmap glyph.
  * Hourly grid cell borders added between rows and columns.
  * Hourly cards show wind speed on a third line (`xx km/h`).
  * Hourly time format changed from 24-hour `HH:MM` to 12-hour `h:MMam/pm`.
  * Forecast precipitation label renamed `Rain` → `Precip` (covers snow).
  * Forecast page shows `"Swipe for more"` hint text next to right triangle when more pages exist.
  * Settings diagnostics reformatted as two-column label/value layout; `Last synced` row added.
  * `_drawLastUpdated` now called on every `renderActivePage` render (previously implemented but unused).
  * `DisplayManager::setLastSyncTime(time_t)` added; called from `AppController` after each render.
  * Pagination dots now show active page name label above the dot strip (FreeSans9pt, BC_DATUM).
  * Page switches show a brief horizontal stripe flash for visual swipe feedback.

### Fixed

* **`updateClockOnly` font override** — removed the trailing font-number argument from `drawCentreString` in `updateClockOnly()`. The argument silently selected the built-in tiny bitmap glyph on every minute tick, overriding the `FreeSansBold24pt7b` font that had just been set. Clock now renders in the correct large bold font.
* **Tomorrow preview label** — renamed `"Rain: xx%"` → `"Precip: xx%"` in the Dashboard tomorrow strip to match the Forecast cards and correctly represent snow accumulation.
* **`_drawLastUpdated` on Settings page suppressed** — the `"Updated: HH:MM"` timestamp badge (BR_DATUM, Y=955) is no longer drawn on the Settings page, where the `"Last synced: N min ago"` diagnostic row already provides the same information.
* **Details overlay content** — replaced the overlay's UV Index / Visibility / Cloud Cover rows (already visible in the Dashboard details grid) with genuinely new information: AQI value with EPA descriptive category label, full weather alert headline + severity (or a reassurance string when none is active), and an estimated dew point calculated from temperature + humidity.
* **Forecast scroll hint ambiguity** — the `"Swipe for more"` hint at X=40 conflicted with the left-pointing triangle also rendered at X=10–30 when both previous and next forecast pages exist. The hint is now drawn right-aligned (`MR_DATUM`, X = kWidth−42) adjacent to the right-pointing triangle, and is only shown on the first page (`forecastOffset == 0`) where no back-arrow can cause directional confusion.
* **Provisioning screen font inconsistency** — all five text strings (`"Scan to Connect & Configure"`, subtitle, SSID, URL, `"No password required"`) were using `setTextSize` + a trailing font-number argument that selected the tiny built-in bitmap glyph. Replaced with explicit FreeSans font calls (`FreeSansBold18pt7b`, `FreeSans9pt7b`, `FreeSansBold12pt7b`, `FreeSans12pt7b`) consistent with the rest of the UI.
* **WeatherService: supplemental data blocked by current-conditions failure** — a failed current-conditions HTTP GET previously caused an early `return` that skipped all subsequent fetches (AQI, sun times, hourly, alerts). Replaced with a `currentOk` flag; supplemental fetches always proceed regardless, so cached auxiliary data stays fresh during a partial API outage.
* **WeatherService: no HTTP timeout** — all five sequential HTTP fetches could block indefinitely on a dead server, preventing the device from entering deep sleep. Added `http.setTimeout(10000)` (10 s hard limit) on the shared `HTTPClient` instance.
* **WeatherService: `DailyForecast::dayTime` never populated** — the `dayTime` field was declared but never assigned, causing the Forecast page to show a zeroed timestamp for every column. It is now parsed from `interval.startTime` (RFC 3339) via `strptime` / `mktime`.
* **WiFiManager: stale RTC fast-connect cache on timeout** — `connectSTA` and `connectBestSTA` now zero `rtc_bssid` and clear `rtc_cached_ssid[0]` on connection timeout, alongside the existing `rtc_channel = 0`. Previously only the channel was cleared, leaving a stale BSSID that caused the next boot to attempt fast-connect to a dead AP.
* **WebServer: sparse WiFi slot breaks provisioning** — WiFi entry processing used `break` when a param index was missing, silently discarding all higher-indexed networks. Changed to `continue` so gaps in submitted indices are skipped rather than halting the loop.
* **WebServer: SoftAP re-provisioning without PIN verification** — once the async web server was running, any client on the open SoftAP could POST `/save` and overwrite the configuration without supplying the existing PIN. The handler now loads the stored `pin_hash` and, if non-empty, requires a matching `current_pin` parameter. Three consecutive wrong-PIN submissions trigger a 60-second lockout (HTTP 429).
* **ConfigManager: silent NVS write failures** — `_prefs.putInt("w_count")` and `_prefs.putString()` for WiFi credentials and `api_key` now check the return value (bytes written) and emit `ESP_LOGE` when zero is returned, making NVS corruption or flash exhaustion visible in the log.
* **AppController: webhook GET has no timeout** — added `http.setTimeout(5000)` before the webhook `http.begin()` call so an unresponsive server cannot block the interactive session indefinitely.
* **DisplayManager: duplicate `_drawBattery()` call** — `drawPageDashboard()` called `_drawBattery()` at its start, but `renderActivePage()` already calls it before dispatching to any page renderer. Removed the redundant call from `drawPageDashboard()` to prevent a double `analogReadMilliVolts` sample and double-paint of the battery icon.
* **DisplayManager: task watchdog reset missing in PIN loop** — `promptPIN()`'s touch-wait loop never fed the ESP32 task watchdog, which would fire after the configured WDT timeout if the user paused on the keypad. Added `esp_task_wdt_reset()` on every iteration and a 2-minute hard timeout that returns an empty string if no OK is pressed, preventing both a WDT reset and an indefinitely pinned display.
* **main: unlimited PIN retry attempts** — the provisioning PIN gate had no retry limit, allowing brute-force enumeration of short PINs offline. The device now reboots after 3 consecutive wrong PIN entries.

### Changed

* **AppController: EXT0 wakeup setup extracted** — the identical four-line GPIO/RTC block (`rtc_gpio_init` → `rtc_gpio_set_direction` → `rtc_gpio_pulldown_dis` → `esp_sleep_enable_ext0_wakeup`) was duplicated in three sleep-entry paths. Extracted into a `static void _configureEXT0Wakeup()` file-local helper called from all three sites.
* **AppController: `kForecastColsPerView` constant** — the magic literal `3` controlling forecast scroll bounds is replaced by `static constexpr int kForecastColsPerView = 3` to make the relationship between column count and scroll limits self-documenting.
* **AppController: ghost cleanup interval raised 20 → 48** — `kGhostCleanupInterval` increased from 20 to 48 full-quality redraws. The previous value triggered the 1.3-second W→B→W flash too frequently during active scrolling; 48 redraws corresponds to roughly 24 hours of normal 30-minute sync cycles.

---

## [2.0.0] - 2026-03-12

### Added

* **Animated loading screen** — 3-step progress bar (Connecting → Syncing → Fetching) with cloud+sun vector icon, step dots with connectors, and action label. Drawn with `epd_quality` on first sync; partial `epd_fastest` refresh for each step advance.
* **Icon-based settings menu** — replaced text list with a 3-column grid of vector icons: circular-arrow Sync, concentric-arc WiFi, crescent-moon Sleep. Touch tap on each column triggers the action.
* **Touch tap detection** — `InputManager::checkTap(x, y)` detects a touch release that did not cross the 30 px swipe threshold, enabling tap-based menu interaction without conflicting with horizontal swipe scrolling.
* **Settings action feedback** — `showMessage()` confirmation shown before each settings action (Force Sync, Web Setup, Deep Sleep).
* **Force Sync immediate wakeup** — `AppController::_enterDeepSleepForImmediateWakeup()` uses a 1-second timer so the device wakes into the normal timer-fetch path almost immediately, rather than sleeping the full 30-minute interval.
* **Today page enhancements** — wind compass direction appended to wind speed; new UV index and visibility row; sunrise and sunset times displayed flanking the sun arc dial; Tomorrow preview enriched with weather icon, centred layout, and precipitation chance.
* **Forecast page — temperature band sparkline** — dual lines (thick for daily highs, thin for daily lows) with Y-axis, Hi/Lo legend, and degree symbol labels.
* **Forecast page — precipitation bar chart** — new `_drawPrecipBars()` helper renders a 0–100 % vertical bar chart for all 10 forecast days, aligned to the same X grid as the temperature sparkline.
* **Forecast cards** — real weekday names derived from `DailyForecast::dayTime`; weather icon per card; temperature range bar contextualised against the 10-day span; always-visible precipitation chance; vertical column dividers.
* **Last-known IP caching** — `rtcLastIP` stored in `RTC_DATA_ATTR` and populated after each successful WiFi connect. Settings diagnostics now shows live IP, cached IP with `(offline)` badge, or `No data yet` appropriately.

### Fixed

* **EXT0 (G38) wakeup never fired** — `rtc_gpio_init()` and `rtc_gpio_set_direction()` must be called to move the pin from the GPIO-matrix domain into the RTC IO domain before `esp_sleep_enable_ext0_wakeup()`; the missing call silently prevented any wakeup. Also removed `rtc_gpio_pullup_en()` which silently fails for GPIO34–39 (input-only pins with no internal pull resistors on ESP32).
* **Screen stuck on "Fetching weather"** — EXT0 wakeup path with no cached data now shows `showMessage("No Data Yet", ...)` instead of rendering the "Fetching…" placeholder with no fetch in progress. `renderActivePage` in the interactive session is guarded by `rtcCachedWeather.valid`.
* **Force Sync stuck on "Syncing…"** — Force Sync previously called `enterDeepSleep()` (30-minute timer), causing the device to sleep through the action. Now calls `_enterDeepSleepForImmediateWakeup()` (1-second timer).
* **Battery voltage displayed as 0.00 V** — Settings diagnostics used `M5.Power.getBatteryVoltage()` which always returns 0 on this hardware revision. Replaced with `_cachedBatVoltage` / `_cachedBatLevel` populated by `analogReadMilliVolts(35)`.
* **Settings IP always showing "Offline"** — WiFi is not connected during interactive (button-wakeup) sessions; IP is now cached in RTC memory from the last fetch cycle for accurate diagnostics display.

### Changed

* Settings page actions are now triggered by screen tap (column-X hit-test, `kSettingsColW = 180`) rather than G38 rocker click.
* Cloud+sun loading icon uses fill→hollow technique (solid fill of all lobes first, then white inset fill) to eliminate visible interior arc seams that appeared when drawing individual circle outlines.
* Forecast page detail cards replaced generic "Day N" labels with formatted weekday strings (`Mon 12`).

---

## [1.0.1] - 2026-03-08

### Added

* Comprehensive Doxygen API documentation across all 9 core `lib/` header files, improving overall project maintainability and developer onboarding.

### Changed

* Swapped non-standard `#pragma once` parser directives for robust C++ `#ifndef` macro include guards across all library headers to improve compilation portability on the Xtensa architecture.

---

## [1.0.0] - 2026-03-08

### Added

* Initial public release of the M5Paper ESP32 Weather Monitor.
* Integrated Google Weather API `v1` implementation parsing current conditions and 10-day forecasts (`pageSize=10`).
* Added POSIX-compliant NTP time synchronisation and timezone management mapping `localtime_r()` directly to ESP-IDF.
* 10-day forecast UI natively parsed onto the 4.7" e-ink device canvas.
* Provisioning Web Server using `ESPAsyncWebServer` running a captive portal setup page.
* Extensive Markdown documentation added in the `docs/` directory (`architecture.md` and `features.md`).

### Changed

* Complete architectural extract of monolithic source files from `src/` into 6 distinct PlatformIO `lib/` modules (`App`, `Config`, `Display`, `Input`, `Network`, `Provisioning`).
* Updated all core code to use universal library include paths (`#include <Header.h>`).
* Dramatically sped up the swiping mechanism in `InputManager` to eliminate the touch-release lag on continuous horizontal page scrolls.
* UI formatted to consistently centre the combined string `<City>, <State>`.
