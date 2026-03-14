# M5Paper Weather Monitor

[![PlatformIO CI](https://github.com/arunkumar-mourougappane/esp32-weather-monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/arunkumar-mourougappane/esp32-weather-monitor/actions/workflows/ci.yml)
[![GitHub Release](https://img.shields.io/github/v/release/arunkumar-mourougappane/esp32-weather-monitor?include_prereleases&sort=semver)](https://github.com/arunkumar-mourougappane/esp32-weather-monitor/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Core_6-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

A live, e-ink weather monitor and desk clock built for the **M5Stack Paper** (ESP32). It features smartphone-based provisioning via a captive portal, live weather from the Google Weather API, a scrollable 10-day forecast, and optimized e-ink refresh for long-term reliability.

---

## Features

| Feature                           | Description                                                                                                          |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| **High-Quality E-ink Display**    | `epd_quality` full refresh for new weather data; `epd_fastest` partial refresh for clock ticks and loading steps     |
| **Today Dashboard**               | Time, date, hero icon + temperature, 3-row details grid (feels-like, wind+bearing, humidity, clouds, UV, visibility) |
| **Environmental Dials**           | AQI half-arc gauge with needle; astronomical Sun Arc plotting the sun's position with flanking sunrise/sunset times  |
| **Tomorrow Preview**              | Centred card with weather icon, condition, H/L temperatures, and precipitation chance                                |
| **Scrollable 10-Day Forecast**    | Swipe through 3-per-view cards showing weekday, icon, H/L, temp range bar, and rain chance                          |
| **Temperature Band Sparkline**    | Dual-line chart (thick Hi, thin Lo) across all 10 days with Y-axis and Hi/Lo legend                                 |
| **Precipitation Bar Chart**       | Vertical bar chart showing rain probability (0–100 %) for each forecast day, aligned to the same day grid           |
| **Icon-Based Settings Menu**      | 3-column vector icon grid (Sync / Setup / Sleep); tap the column to trigger the action                              |
| **Animated Loading Screen**       | 3-step progress (WiFi → NTP → Weather) with cloud+sun illustration and per-step fast partial refresh                |
| **Touch Tap & Swipe Gestures**    | Swipe (≥30 px delta) scrolls the forecast; tap drives the settings menu without interfering with swipe detection    |
| **Google Weather API (v1)**       | Current conditions + 10-day forecast (`pageSize=10` bypasses the default 5-day limit)                               |
| **Open-Meteo APIs (Free)**        | Unauthenticated AQI (US EPA) and daily ephemeris (sunrise/sunset) supplementing the core forecast                   |
| **Smart Provisioning Portal**     | On first boot, creates an AP + QR code; scan to open the captive portal at `192.168.4.1`                            |
| **Multi-Network WiFi Roaming**    | Store up to 5 SSID/password pairs; on each wake the device scans, ranks by RSSI, and connects to the strongest AP   |
| **Encrypted NVS Credentials**     | WiFi passphrases, API key, and webhook URL are AES-256-CTR encrypted in NVS; key is device-unique (eFuse MAC)        |
| **12-Hour Clock**                 | Local time in AM/PM format, synced via NTP and preserved offline by the BM8563 hardware RTC crystal                 |
| **Configurable Timezone**         | Dropdown of US and world timezones in the provisioning portal — no POSIX strings required                            |
| **Secure Configuration**          | Sensitive credentials stored in ESP32 NVS and AES-256-CTR encrypted; key derived from factory eFuse MAC (hardware-bound) |
| **Provisioning PIN Lock**         | Optional 4–8 digit PIN (SHA-256 hashed) to gate the setup portal                                                    |
| **Hardware Button Reset**         | Hold G38 at boot to re-enter provisioning mode; short press wakes from deep sleep into interactive mode             |
| **Force Sync**                    | Tap the Sync icon in Settings to immediately queue a fresh fetch; device wakes within 1 second via the normal path  |
| **Last-Known IP Diagnostics**     | Cached IP survives deep sleep; Settings shows live IP, offline-cached IP, or "No data yet"                         |
| **Hardware Battery Tracking**     | Bypasses broken M5Unified abstractions — reads `analogReadMilliVolts(35)` directly for accurate LiPo gauging        |
| **Deep Sleep & Battery Life**     | Halts execution between 30-minute syncs; full weather + forecast cached in `RTC_DATA_ATTR` for instant button-wake  |

---

## Hardware Requirements

- **M5Stack Paper** (M5Paper V1.1 or compatible ESP32 e-paper device)
- USB-C cable for flashing
- LLVM or clang
- PlatformIO installed (VS Code extension or CLI)

---

## Software Stack

| Library               | Purpose                                                     |
| --------------------- | ----------------------------------------------------------- |
| **M5Unified / M5GFX** | Display driving, e-ink refresh modes                        |
| **ESPAsyncWebServer** | Provisioning captive portal                                 |
| **ArduinoJson**       | Google Weather & Open-Meteo API JSON parsing                |
| **QRCode**            | QR code generation for pairing                              |
| **ESP32 SNTP / NTP**  | Time synchronization via `pool.ntp.org`                     |
| **mbedTLS**           | SHA-256 hashing for provisioning PIN; AES-256-CTR + Base64 for NVS credential encryption |

---

## First Time Setup

1. **Flash the firmware** using PlatformIO:

   ```bash
   pio run -t upload
   ```

2. **Boot the device** — it will display a QR code and an SSID name.
3. **Scan the QR Code** with your phone. Connect to the `WeatherSetup` AP and the provisioning page will open.
4. **Enter your configuration**:
   - **WiFi Networks**: Add up to 5 SSID/password pairs — tap **+ Add Network** for additional entries
   - **Google Weather API Key** (Google Cloud key with Weather API enabled)
   - **Location**: City display name, State (optional), Country (ISO), Latitude, Longitude
   - **Timezone**: Select your timezone from the dropdown (US, Europe, Asia/Pacific)
   - **NTP Server**: Defaults to `pool.ntp.org`
   - **Security PIN** (optional): 4–8 digit PIN to lock the setup portal
5. Tap **Save & Restart** — the device reboots, connects to WiFi, syncs NTP time, and displays the weather dashboard.

---

## Controls

| Input                                      | Action                                                      |
| ------------------------------------------ | ----------------------------------------------------------- |
| **Click wheel button (G38)**               | Wake device from deep sleep into 10-minute interactive mode |
| **Swipe left on touchscreen**              | Scroll forecast forward (newer days)                        |
| **Swipe right on touchscreen**             | Scroll forecast back (earlier days)                         |
| **Tap Settings icon column**               | Trigger Sync / Web Setup / Sleep action                     |
| **Multi-function wheel scroll up** (G37)   | Scroll forecast forward (newer days)                        |
| **Multi-function wheel scroll down** (G39) | Scroll forecast back                                        |
| **Hold wheel button (G38) at boot**        | Enter provisioning / settings mode                          |

---

## Re-Provisioning

To change WiFi, API key, location, or timezone:

1. Hold the **multi-function wheel button (G38)** while the device boots.
2. If a PIN was configured, enter it on the e-ink keypad.
3. The provisioning QR code will appear — scan and update your settings.

---

## Architecture Overview

```text
src/
└── main.cpp                          # Boot decision: provisioning vs. normal
lib/
├── App/
│   └── AppController.cpp/h           # Synchronous event-driven state machine & Deep Sleep orchestration
├── Config/
│   └── ConfigManager.cpp/h           # NVS-backed settings (timezone, WiFi, API key…)
├── Display/
│   └── DisplayManager.cpp/h          # M5GFX wrapper: weather UI, forecast, clock
├── Input/
│   └── InputManager.cpp/h            # G38 pin watch, wheel scroll, touch PIN entry
├── Network/
│   ├── NTPManager.cpp/h              # Time sync with POSIX timezone support
│   ├── WeatherService.cpp/h          # Google Weather API HTTP client
│   └── WiFiManager.cpp/h             # STA + AP mode management
└── Provisioning/
    ├── ProvisioningManager.cpp/h     # AP + web server orchestration
    ├── WebServer.cpp/h               # AsyncWebServer routes
    └── html/provision.h              # Embedded dark-themed HTML setup form
```

---

## Configuration Reference

| NVS Key        | Description                          | Example / Notes              |
| -------------- | ------------------------------------ | ---------------------------- |
| `w_count`      | Number of stored WiFi networks       | `1`–`5`                      |
| `w_ssid_N`     | SSID for network slot N (0–4)        | `MyHomeNetwork`              |
| `w_pass_N`     | Password for network slot N (0–4)    | AES-256-CTR encrypted        |
| `wifi_ssid`    | Legacy single-network SSID (v1 compat) | `MyHomeNetwork`            |
| `wifi_pass`    | Legacy single-network password       | AES-256-CTR encrypted        |
| `api_key`      | Google Weather API key               | AES-256-CTR encrypted        |
| `city`         | Display city name                    | `Chicago`                    |
| `state`        | State/province (optional)            | `Illinois`                   |
| `country`      | ISO 3166-1 alpha-2                   | `US`                         |
| `lat`          | Latitude                             | `41.8781`                    |
| `lon`          | Longitude                            | `-87.6298`                   |
| `timezone`     | POSIX TZ string                      | `CST6CDT,M3.2.0,M11.1.0`    |
| `ntp_server`   | NTP server hostname                  | `pool.ntp.org`               |
| `webhook_url`  | Optional HTTP webhook URL            | AES-256-CTR encrypted        |
| `pin_hash`     | SHA-256 of PIN                       | (auto-generated)             |
| `cfg_encv`     | Credential encryption migration flag | `true` once migrated         |
