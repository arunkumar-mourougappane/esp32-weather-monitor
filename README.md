# M5Paper Weather Monitor

A live, e-ink weather monitor and desk clock built for the **M5Stack Paper** (ESP32). It features an elegant smartphone-based provisioning system via captive portal, live weather fetching using the Google Weather API, and optimized e-ink refresh modes for long-term reliability.

## Features

- **High-Quality E-ink Display**: Uses `epd_quality` full refresh for new weather data and `epd_fastest` partial refresh to tick the clock every minute without full-screen flashing.
- **Smart Provisioning**: On first boot, the device creates an Access Point (AP) and displays a QR code. Scan it with a smartphone to join the AP and open a beautiful captive portal to configure your WiFi, API keys, Location, and Timezone.
- **Google Weather API (v1)**: Fetches live temperature, feels-like temperature, humidity, wind, and UV index.
- **Dynamic Loading Screen**: Provides visual feedback on boot while first data syncs.
- **Secure Configuration**: Uses native ESP32 NVS (Non-Volatile Storage) to store settings.
- **PIN Lock Feature**: Optional PIN code protection (hashed securely via SHA-256) on boot before displaying weather.
- **Hardware Button Reset**: Hold the multi-function wheel push button (`G38`) for 10 seconds to factory reset and re-enter provisioning mode.

## Hardware Requirements

- **M5Stack Paper** (M5Paper V1.1 or similar ESP32 E-paper device)
- PlatformIO ecosystem (`platformio.ini` configured for `m5stack_paper`)

## Software Configuration

This project is built using:

- **PlatformIO** with the Arduino framework
- **M5Unified** and **M5GFX** for display driving
- **ESPAsyncWebServer** and **ArduinoJson** for handling the provisioning portal and API parsing
- **QRCode** for generating pairing codes

## First Time Setup

1. **Flash the firmware** using PlatformIO:

   ```bash
   pio run -t upload
   ```

2. **Boot the device**. It will display a QR code.
3. **Scan the QR Code** with your phone. You will be connected to the `WeatherSetup` network and redirected to the provisioning page.
4. **Enter your configuration**:
   - **WiFi Credentials**: So the M5Paper can connect to the internet.
   - **Google API Key**: Note: The project expects a Google Cloud API Key with the Weather/Environment APIs enabled.
   - **Location Data**: City, State (optional), Country, Latitude, and Longitude.
   - **Timezone**: POSIX timezone format (e.g. `CST6CDT,M3.2.0,M11.1.0`).
   - **Hardware PIN** (optional): Set a 4-8 digit lock screen PIN.
5. Tap **Save**. The M5Paper will reboot, connect to your WiFi, sync NTP time, and fetch your beautiful live weather dashboard!

## Resetting

To change your Wi-Fi or API settings, reboot the device and hold the multi-function wheel button IN (GPIO 38) for 10 seconds. The device will reset its NVS configuration and reboot back to the QR code setup screen.
