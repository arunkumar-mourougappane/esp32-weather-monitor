# Changelog

All notable changes to the `esp32-weather-monitor` project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
