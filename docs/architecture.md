# System Architecture

The M5Paper Weather App is built on top of the ESP32 framework (`framework-arduinoespressif32`) using a strict Object-Oriented, task-driven architecture. The application decouples hardware interfacing, network operations, UI rendering, and state management into distinct Singleton manager classes.

## 🏗️ Core Components

### 1. `AppController` (The Orchestrator)

The brain of the application. It acts as a synchronous, event-driven state machine orchestrating network fetches, UI rendering, and ultra-low power states.

- **Deep Sleep Lifecycles**: Drifts the ESP32 into a hardware hibernation state between 30-minute sync windows to maximize battery longevity.
- **RTC Fast-Wake**: Stores the entire JSON object tree in `RTC_DATA_ATTR` non-volatile memory, allowing the device to wake via hardware button clicks, render instantly offline, and drift back to sleep.

### 2. Network Layer

Responsible for all outbound and inbound connectivity.

- **`WiFiManager`**: Dynamically toggles the ESP32 between Station (STA) mode for regular Internet access, and SoftAP mode during initial setup.
- **`NTPManager`**: Bypasses traditional `delay()` loops by utilizing the ESP-IDF native `configTzTime` locale system. It explicitly wipes stale RTC (Real-Time Clock) data on cold boots and guarantees synchronization with `pool.ntp.org` to seamlessly handle tricky Daylight Saving Time transitions.
- **`WeatherService`**: Handles outbound TLS connections to the Google Weather API. Uses `ArduinoJson` (v7) to safely parse the returned arrays, dynamically bypassing 5-day endpoint limits by artificially expanding the polling `pageSize=10` query parameter.

### 3. Hardware & Display Layer

Abstracts the physical M5Paper interfaces from the core logic.

- **`DisplayManager`**: Built on `M5GFX`, it handles all graphics. Instead of clearing the screen globally (which causes an agonizing black/white flash on e-ink), it leverages bounding boxes to draw high-speed partial updates. It also handles **Native Battery Gauge Optimization** by mapping raw `analogReadMilliVolts(35)` signals across a 1/2 hardware voltage divider to seamlessly circumvent broken ESP32 internal framework abstractions.
- **`InputManager`**: Ties into the M5Paper's GT911 capacitive touch panel and the multi-function rocker switch (Pins G37/G38/G39). Translates wheel interactions into horizontal forecast scrolling or Deep Sleep hardware wakeups.

### 4. Storage & Provisioning

- **`ConfigManager`**: Interfaces with the ESP32's `Preferences.h` lib to read/write structured blocks to the Non-Volatile Storage (NVS) partition. This ensures City, State, API keys, and Wi-Fi credentials outlive cold boots or battery deaths.
- **`ProvisioningManager` & `WebServer`**: When the device detects no saved configuration (or if the user holds the rocker switch), it spins up an asynchronous web server (`ESPAsyncWebServer`) serving a heavily styled HTML dashboard directly from flash memory. Once the user submits their settings, the device saves to NVS and auto-reboots.

## 🔄 Execution Flow

1. **Boot:** `main.cpp` initializes `M5` and `ConfigManager`.
2. **Check:** If NVS is empty, enter SoftAP mode and launch `ProvisioningManager`.
3. **Connect:** If NVS is populated, start `WiFiManager` to connect to the router.
4. **Sync:** `NTPManager` blocks until real atomic UTC time is confirmed.
5. **Fetch & Render:** `AppController` triggers a synchronous download of the weather payload, formats the UI, and paints the e-ink display.
6. **Sleep:** The ESP32 physically kills power to its own Wi-Fi and Touch hardware, entering Deep Sleep until the 30-minute timer or G38 hardware pin explicitly wakes it back to Step 1.
