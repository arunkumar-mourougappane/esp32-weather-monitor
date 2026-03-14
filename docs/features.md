# Functionalities & Features

This document outlines the core user-facing functionalities implemented in the M5Paper Weather App.

## 🌤️ Weather Forecasting Engine

* **10-Day Deep Lookahead**: The device specifically overrides standard Google Weather API pagination caps (which default to 5 days) by enforcing `pageSize=10`. This allows the application to cleanly parse 10 contiguous days of highs, lows, precipitation chances, and prevailing condition states in a single JSON block.
* **Live Ambient Conditions**: Fetches real-time localised temperature, relative humidity, dynamic "feels like" temperature, wind speed with 8-point compass direction, UV index, visibility, and cloud cover.
* **Supplemental Environmental APIs**: While Google Weather drives the core forecast, the engine initiates synchronous unauthenticated Open-Meteo HTTP fetches to derive real-time US EPA Air Quality Index (AQI) values and track daily ephemeris (sunrise and sunset) timestamps without incurring API-key bottlenecks.
* **Dynamic Screen Formatting**: Given that city names range from `"Rome"` to `"Llanfairpwllgwyngyll"`, the application engine natively concatenates the optional State parameter and uses hardware-accelerated bounding boxes (`drawCentreString`) to ensure the locale is perfectly horizontally aligned regardless of string length.

## 📱 Multi-Page E-Ink UI

### Today Page

Displays the current conditions dashboard:

* Large time and date header with full weekday, month, and year.
* Hero section: vector weather icon alongside temperature.
* **Details grid** (3 rows): Feels Like / Wind speed + compass bearing; Humidity / Cloud cover; UV index / Visibility.
* **Wind rose compass**: 8-point compass visualization showing prevailing wind direction with speed indicator.
* **Moon Phase Widget**: Display fractional moon phase derived from unixtime using shading logic.
* **Environmental dials**: AQI half-arc gauge with needle, and astronomical Sun Arc showing the sun's position across the day with flanking sunrise and sunset times.
* **Tomorrow preview**: centred card with weather icon, condition text, high/low temperatures, and precipitation chance.

### Hourly Forecast Page (New)

* Complete 24-hour strip displaying the time, vector weather condition icon, temps, and precipitation chance.

### Swipe-up detail overlay

* Swipe vertically on the touch screen to reveal additional information seamlessly without changing horizontal pages.

### 10-Day Forecast Page

* **Temperature band sparkline**: dual-line chart plotting daily highs (thick) and lows (thin) across all 10 days with Y-axis, Hi/Lo legend, and degree labels.
* **Precipitation bar chart**: vertical bar chart showing rain probability (0–100 %) per day, aligned to the same horizontal grid as the temperature chart.
* **Scrollable forecast cards** (3 visible at a time, swipe to scroll): real weekday name from timestamp (`Mon 12`), vector weather icon, condition text, H/L temperatures, temperature range bar contextualised against the full 10-day span, and precipitation chance.

### Settings & Diagnostics Page

* **3-column icon grid**: vector Sync, WiFi, and Sleep icons — touch the column to trigger the action.
* **Diagnostics row**: battery voltage and percentage (from hardware-accurate ADC), firmware version, and last-known IP address with live/offline status badge.

## ⚙️ Seamless Device Management

* **Configurable sync interval**: The deep-sleep timer duration is configurable via the AP provisioning portal; the hardcoded 30-minute default can be raised or lowered at runtime.
* **Battery-adaptive sync rate**: If the battery voltage drops below 40% (≈3650 mV), the synchronization interval automatically doubles to preserve power without user intervention.
* **Double-click webhook**: A physical double-tap of G38 fires an HTTP GET to a user-configured webhook URL and displays a brief confirmation on screen. Configurable in the provisioning portal.
* **Multi-Network WiFi Roaming**: Up to 5 SSID/password pairs can be stored in NVS. On each wake the device scans for available access points, ranks matching SSIDs by received signal strength (RSSI), and connects to the strongest available network automatically. A fast-connect cache (BSSID + channel) in `RTC_DATA_ATTR` is keyed to the last successfully connected SSID; if it is still in range it is tried first to skip the channel scan.
* **AES-256-CTR NVS Credential Encryption**: All sensitive credentials persisted to non-volatile storage — WiFi passphrases, the API key, and the webhook URL — are encrypted with AES-256 in CTR mode (mbedTLS) before being written. The 256-bit key is derived from the device's factory-burned eFuse MAC address via SHA-256, making the ciphertext hardware-bound (an NVS dump cannot be replayed on a different chip). A 16-byte random IV from the ESP32 hardware TRNG is generated on every write; the stored value is `"E1:" + Base64(IV ∥ ciphertext)`. Devices upgraded from older firmware are migrated automatically on first boot — no re-provisioning required.
* **Secured Re-Provisioning**: When a PIN is set, POST `/save` requires the existing PIN as `current_pin`. Three consecutive wrong submissions trigger a 60-second lockout (HTTP 429). The device reboots after 3 wrong PIN entries on the e-ink keypad, preventing offline brute-force.
* **Zero-Code Setup Portal**: Users never hardcode Wi-Fi credentials or API tokens. The device broadcasts its own AP and serves a responsive mobile web app at `192.168.4.1` where up to 5 WiFi networks can be added, reordered, and removed. Settings are persisted to NVS.
* **Hardware Re-Provisioning**: Holding G38 at boot forces an immediate reboot into AP/provisioning mode without risking NVS corruption.
* **Force Sync**: Tap the Sync icon in Settings to immediately queue a fresh weather fetch; the device sleeps for 1 second and wakes via the normal timer-fetch path.
* **Settings touch navigation**: all settings actions are triggered by capacitive tap on the corresponding icon column — no button press required.
* **Atomic Time Correctness**: Invalid or stale RTC timestamps from deep sleep states are proactively detected; the application strictly halts until `pool.ntp.org` returns verified UTC time.

## 🎬 Animated Loading Screen

When the device has no cached data and initiates a first-time fetch, a full-quality splash is displayed showing:

* City name header.
* Cloud+sun vector icon (rendered using a fill→hollow technique to produce a clean outline with no internal arc seams).
* 3-step animated progress indicator: Connecting to WiFi → Syncing time → Fetching weather, with a progress bar, step dots, checkmarks for completed steps, and a current action label. Each step is advanced with a fast partial refresh so the static art never redraws.

## 🔋 Advanced Power Management

* **Ultra-Low Power Deep Sleep**: The ESP32 physically halts execution between 30-minute weather sync windows. Standard FreeRTOS tasks are replaced by an event-driven wakeup lifecycle.
* **Intelligent Timer Wakeup**: On timer wakeup the device connects to WiFi, fetches weather, renders the full display, and immediately returns to sleep.
* **Button Wakeup (G38 / EXT0)**: Pressing G38 wakes the device into a 10-minute interactive session. The display renders immediately from the RTC cache with no network round-trip. The EXT0 source requires `rtc_gpio_init()` to transfer the pin into the RTC IO domain before `esp_sleep_enable_ext0_wakeup()` is called.
* **RTC Interactive Cache**: The entire `WeatherData` struct (weather + 10-day forecast) is stored in `RTC_DATA_ATTR` fixed-size buffers — not heap Strings — so it survives deep sleep and is available instantly on button wakeup.
* **Native Battery Gauge**: Bypasses broken M5Unified power abstractions by sampling the 1/2 voltage divider on GPIO 35 directly via `analogReadMilliVolts(35)`. Produces accurate LiPo drain visualisations on the display.

## 📡 Connectivity & Diagnostics

* **Last-Known IP Persistence**: The IP address assigned during the last successful WiFi connection is stored in `RTC_DATA_ATTR`. The Settings page displays the live IP when online, the cached IP with an `(offline)` badge during interactive mode, or `No data yet` on first boot.
* **Fluid E-Ink Refresh Pipeline**: `epd_quality` mode is used for full-page weather redraws; `epd_fastest` mode is used for clock ticks, step-progress updates, and forecast scrolling to avoid full-screen flashes.
* **Instant Touch Response**: Input engine tracks delta-X movements in real time, triggering swipe thresholds mid-drag rather than on finger lift, for an instantaneous UI feel. Tap detection (release without crossing 30 px) drives the settings menu independently of swipe gesture recognition.
