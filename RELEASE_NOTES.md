# esp32-weather-monitor v2.0.0 Release Notes

See the full release notes in [`docs/releases/v2.0.0.md`](docs/releases/v2.0.0.md).

This is a major feature release adding a significantly richer UI, touch-driven navigation, an animated loading screen, two new forecast charts, and a set of hardware bug fixes that were silently preventing correct device behaviour.

## 🚀 Highlights

* Fully reworked display UI across all three pages — Today, 10-Day Forecast, and Settings & Diagnostics
* Touch-driven settings menu with vector icon grid (Sync / Setup / Sleep)
* Animated 3-step loading screen with cloud+sun illustration
* Two new forecast charts: temperature band sparkline (Hi + Lo lines) and precipitation bar chart
* Critical hardware fix — G38 button wakeup from deep sleep now works correctly
* Accurate diagnostics — battery voltage and last-known IP now always show real data
* Force Sync now wakes the device immediately (1-second timer) instead of waiting 30 minutes

## 🐛 Critical Fixes

* **G38 wakeup silent failure** — `rtc_gpio_init()` was missing before `esp_sleep_enable_ext0_wakeup()`; the pin was never transferred into the RTC IO domain so button presses were invisible to the deep-sleep peripheral.
* **Screen stuck on "Fetching weather"** — EXT0 wakeup with no cached data now shows a proper "No Data Yet" message.
* **Force Sync stuck on "Syncing…"** — was using 30-minute sleep timer; now uses 1-second timer.
* **Battery shows 0.00 V** — replaced broken `M5.Power.getBatteryVoltage()` with `analogReadMilliVolts(35)` path.
* **Settings IP always "Offline"** — last IP cached in `RTC_DATA_ATTR` and persisted through deep sleep.
