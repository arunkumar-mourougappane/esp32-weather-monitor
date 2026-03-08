# Release Notes - v1.0.0

Welcome to the **M5Paper Weather App v1.0.0** release! This is the initial stable release of a feature-rich, low-power weather monitoring solution built specifically for the ESP32-powered M5Paper E-ink device.

## 🌟 Key Features

### Beautiful E-Ink Display

* **High-Contrast UI:** Leverages the M5Paper's 4.7" e-ink display for crisp text, clean graphs, and excellent daylight readability.
* **Fast Partial Refreshes:** The app utilizes optimized, dynamic partial screen updates. The clock ticks every minute (and the forecast UI scrolls horizontally) smoothly without forcing a harsh full screen flash.
* **Auto-Centering Location:** Your configured `City` and `State` (e.g., *Peoria, IL*) are automatically fully-centered horizontally, dynamically adjusting dynamically based on string length.

### Comprehensive Weather Insights

* **Current Conditions:** Easily view your real-time temperature, humidity string, "feels like" temp, precipitation probabilities, and wind speeds.
* **10-Day Scrollable Forecast:** Automatically requests 10 days of forecast data from the Google Weather API (with robust pagination bypassing) showing High/Low temps and precipitation chance day-by-day.
* **Touch & Hardware Scrolling:** Navigate the 10-day forecast horizontally via the physical side multi-function wheel, or interactively swipe the capacitive touchscreen. Swiping triggers instantly on pixel tracking, maximizing responsiveness.

### Robust Timekeeping

* **Atomic Sync:** Employs built-in Network Time Protocol (NTP) utilizing native POSIX Timezones (via `pool.ntp.org`) directly out of the box.
* **Timezone & DST Awareness:** Automatic local shifts between Standard and Daylight Saving Time (DST).
* **Real-time Clock Validation:** Guarantees that stale or invalid hardware Real-Time Clock (RTC) timestamps from deep sleep states are proactively wiped before requesting guaranteed, fresh network time.

### Smart Connectivity & Provisioning

* **Captive Web Portal:** Boot up straight into AP (Access Point) Mode on the very first run. A beautifully styled, mobile-optimized HTML portal will appear at `192.168.4.1`, allowing you to insert critical settings in an intuitive UI.
* **Non-Volatile Storage (NVS):** Stores your connected Wi-Fi SSID, network password, custom Location properties, and Google API Key in secure NVS.
* **Re-Provisioning Trigger:** Need to change locations or Wi-Fi networks? Push physical button G38 any time. The ESP32 will reboot, drop back into AP/Provisioning Mode, and launch the portal while retaining your previous settings.

## 🛠 Under the Hood

* Built using PlatformIO, leveraging `framework-arduinoespressif32`.
* Asynchronous Web Servers (`ESPAsyncWebServer`) efficiently handle the Captive Portal requests without freezing graphics threads.
* Highly uncoupled architecture separating Display, App logic, Input handling, Component Networks, and Configuration persistence in clean OOP layers.
