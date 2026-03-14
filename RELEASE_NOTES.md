# esp32-weather-monitor v3.0.0 Release Notes

See the full release notes in [`docs/releases/v3.0.0.md`](docs/releases/v3.0.0.md).

This is a major release combining quality-of-life UI improvements, multi-network WiFi roaming, AES-256-CTR NVS credential encryption, and a comprehensive set of security and robustness fixes.

## 🚀 Highlights

* **AES-256-CTR NVS encryption** — all sensitive credentials are hardware-bound and encrypted at rest; seamless one-time migration on first boot
* **Multi-network WiFi roaming** — store up to 5 SSIDs, ranked by RSSI on each wake; fast-connect cache keyed per SSID
* **Security hardening** — re-provisioning PIN verification, 3-strike lockout, brute-force reboot guard
* **Hourly Forecast page** — full 24-hour strip with icons, temps, precipitation, and wind
* **Swipe-up detail overlay** — dew point, AQI with EPA category label, active weather alert
* **Moon phase widget & wind rose compass** on the Dashboard
* **Configurable sync interval & battery-adaptive rate**
* **Double-click webhook** trigger via physical G38 button

## 🐛 Critical Fixes

* **WeatherService: supplemental fetches blocked on primary failure** — AQI, sun times, hourly, and alerts all now proceed even when the current-conditions call fails.
* **WeatherService: no HTTP timeout** — `http.setTimeout(10000)` added; device can no longer be held awake indefinitely by a dead server.
* **WiFiManager: stale RTC fast-connect cache** — `rtc_bssid` and `rtc_cached_ssid` are now fully zeroed on connection timeout, preventing a perpetually failing fast-connect entry.
* **WebServer: re-provisioning without PIN** — attacker on open SoftAP could overwrite config; existing PIN must now be submitted as `current_pin`; 3-strike 60-second lockout enforced.
* **DisplayManager: task watchdog in PIN loop** — `esp_task_wdt_reset()` added; 2-minute idle timeout prevents WDT reset and indefinitely locked display.
* **main: unlimited PIN brute-force** — device now reboots after 3 consecutive wrong PIN entries.

## ⬆️ Upgrade Notes

1. On first boot after flashing, `ConfigManager` automatically re-encrypts any legacy plaintext NVS credentials — no re-provisioning required.
2. If a provisioning PIN was set in v2.x, the portal `/save` handler now requires it as `current_pin` when reconfiguring.
3. The `APP_VERSION` build flag is `3.0.0`.
