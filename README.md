# M5Paper Weather Monitor

A live, e-ink weather monitor and desk clock built for the **M5Stack Paper** (ESP32). It features smartphone-based provisioning via a captive portal, live weather from the Google Weather API, a scrollable 10-day forecast, and optimized e-ink refresh for long-term reliability.

---

## Features

| Feature                        | Description                                                                                                    |
| ------------------------------ | -------------------------------------------------------------------------------------------------------------- |
| **High-Quality E-ink Display** | `epd_quality` full refresh for new weather data; `epd_fastest` partial refresh for the clock tick every minute |
| **Scrollable 10-Day Forecast** | 3-day window scrollable via the multi-function wheel (scroll up/down); only the forecast region refreshes      |
| **Smart Provisioning Portal**  | On first boot, creates an AP + QR code; scan to open the captive portal                                        |
| **Google Weather API (v1)**    | Fetches temperature, feels-like, humidity, wind, UV index, visibility, cloud cover                             |
| **12-Hour Clock**              | Displays local time in AM/PM format, strictly synced via robust NTP (bypasses stale hardware RTC)              |
| **Configurable Timezone**      | Dropdown of US and world timezones in the provisioning portal — no POSIX strings required                      |
| **Secure Configuration**       | All settings stored in ESP32 NVS (Non-Volatile Storage)                                                        |
| **Provisioning PIN Lock**      | Optional 4–8 digit PIN (SHA-256 hashed) to gate setup mode                                                     |
| **Hardware Button Reset**      | Hold multi-function wheel button (G38) at boot to re-enter provisioning                                        |
| **Dynamic Loading Screen**     | Visual feedback while the device boots and syncs first data                                                    |

---

## Hardware Requirements

- **M5Stack Paper** (M5Paper V1.1 or compatible ESP32 e-paper device)
- USB-C cable for flashing
- PlatformIO installed (VS Code extension or CLI)

---

## Software Stack

| Library               | Purpose                                 |
| --------------------- | --------------------------------------- |
| **M5Unified / M5GFX** | Display driving, e-ink refresh modes    |
| **ESPAsyncWebServer** | Provisioning captive portal             |
| **ArduinoJson**       | Google Weather API JSON parsing         |
| **QRCode**            | QR code generation for pairing          |
| **ESP32 SNTP / NTP**  | Time synchronization via `pool.ntp.org` |

---

## First Time Setup

1. **Flash the firmware** using PlatformIO:

   ```bash
   pio run -t upload
   ```

2. **Boot the device** — it will display a QR code and an SSID name.
3. **Scan the QR Code** with your phone. Connect to the `WeatherSetup` AP and the provisioning page will open.
4. **Enter your configuration**:
   - **WiFi SSID & Password**
   - **Google Weather API Key** (Google Cloud key with Weather API enabled)
   - **Location**: City display name, State (optional), Country (ISO), Latitude, Longitude
   - **Timezone**: Select your timezone from the dropdown (US, Europe, Asia/Pacific)
   - **NTP Server**: Defaults to `pool.ntp.org`
   - **Security PIN** (optional): 4–8 digit PIN to lock the setup portal
5. Tap **Save & Restart** — the device reboots, connects to WiFi, syncs NTP time, and displays the weather dashboard.

---

## Controls

| Input                                      | Action                               |
| ------------------------------------------ | ------------------------------------ |
| **Multi-function wheel scroll up** (G37)   | Scroll forecast forward (newer days) |
| **Multi-function wheel scroll down** (G39) | Scroll forecast back                 |
| **Hold wheel button (G38) at boot**        | Enter provisioning / settings mode   |

---

## Re-Provisioning

To change WiFi, API key, location, or timezone:

1. Hold the **multi-function wheel button (G38)** while the device boots.
2. If a PIN was configured, enter it on the e-ink keypad.
3. The provisioning QR code will appear — scan and update your settings.

---

## Architecture Overview

```
src/
├── main.cpp                          # Boot decision: provisioning vs. normal
├── app/
│   └── AppController.cpp/h           # FreeRTOS task orchestration
├── config/
│   └── ConfigManager.cpp/h           # NVS-backed settings (timezone, WiFi, API key…)
├── provisioning/
│   ├── ProvisioningManager.cpp/h     # AP + web server orchestration
│   ├── WebServer.cpp/h               # AsyncWebServer routes
│   └── html/provision.h             # Embedded dark-themed HTML setup form
├── display/
│   └── DisplayManager.cpp/h          # M5GFX wrapper: weather UI, forecast, clock
├── network/
│   ├── WiFiManager.cpp/h             # STA + AP mode management
│   ├── NTPManager.cpp/h              # Time sync with POSIX timezone support
│   └── WeatherService.cpp/h          # Google Weather API HTTP client
└── input/
    └── InputManager.cpp/h            # G38 pin watch, wheel scroll, touch PIN entry
```

---

## Configuration Reference

| NVS Key      | Description               | Example                  |
| ------------ | ------------------------- | ------------------------ |
| `wifi_ssid`  | Station SSID              | `MyHomeNetwork`          |
| `wifi_pass`  | Station password          | `hunter2`                |
| `api_key`    | Google Weather API key    | `AIza...`                |
| `city`       | Display city name         | `Chicago`                |
| `state`      | State/province (optional) | `Illinois`               |
| `country`    | ISO 3166-1 alpha-2        | `US`                     |
| `lat`        | Latitude                  | `41.8781`                |
| `lon`        | Longitude                 | `-87.6298`               |
| `timezone`   | POSIX TZ string           | `CST6CDT,M3.2.0,M11.1.0` |
| `ntp_server` | NTP server hostname       | `pool.ntp.org`           |
| `pin_hash`   | SHA-256 of PIN            | (auto-generated)         |
