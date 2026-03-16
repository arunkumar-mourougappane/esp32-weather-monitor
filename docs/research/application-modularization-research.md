# Application Modularization & Architecture Research

## 1. The Monolith Problem in Embedded Systems

Currently, the M5Paper Weather Monitor relies heavily on the **Singleton Pattern**, organizing code into module managers (`DisplayManager`, `WiFiManager`, `AppController`). While this provides basic encapsulation, it often leads to **"Singleton Spaghetti"**, where modules become tightly coupled.

* **The Issue:** If `WeatherService` directly calls `DisplayManager::showError()`, the networking layer becomes permanently coupled to the presentation layer. You cannot test the weather logic without pulling in the massive M5GFX e-ink library.
* **The Goal:** Restructure the application so that domains (Networking, UI, Hardware, State) are completely isolated, communicative via standardized contracts or events, and independently testable.

---

## 2. Proposed Architectural Paradigm: Event-Driven Layered Architecture

To achieve true modularization, the system should be divided into distinct structural layers, orchestrated by a central Event Bus or a strict Mediator (`AppController`), rather than cross-calling Singletons.

### A. The 4-Tier Architecture

1. **Hardware Abstraction Layer (HAL):** Wraps all physical ESP32 and M5Paper interactions. (E.g., deep sleep calls, raw I2C touch reads, battery ADC reads, M5GFX raw drawing). The rest of the app never includes `esp_sleep.h` or `M5GFX.h`.
2. **Data & State Layer (The Store):** Plain Old Data (POD) structs. The centralized "source of truth". Contains `RTC_DATA_ATTR` state caches. It does no logic.
3. **Service Layer:** Independent actors that do "work" (e.g., `WeatherFetcher`, `NtpSyncer`, `ConfigLoader`). They read/write to the Data layer but know nothing about the UI.
4. **Presentation Layer:** The Widget and Page system. It observes the Data Layer and renders bounding boxes using the HAL interface.

---

## 3. Decoupling Strategies

### Strategy 1: The Internal Event Bus (Pub/Sub)

Instead of hardcoding the flow sequence, managers emit lightweight events. This allows modules to act independently.

```cpp
// Example of Event-Driven Decoupling
enum SystemEvent {
    EV_WIFI_CONNECTED,
    EV_WEATHER_UPDATED,
    EV_BATTERY_LOW,
    EV_TOUCH_SWIPE_UP
};

// WeatherService simply broadcasts "I'm done"
void WeatherService::fetch() {
    // ... API call ...
    EventBus::publish(EV_WEATHER_UPDATED, &weatherDataPayload);
}

// DisplayCore listens and reacts, without WeatherService knowing about DisplayCore
void DisplayCore::onEvent(SystemEvent event, void* payload) {
    if (event == EV_WEATHER_UPDATED) {
        this->renderWeather((WeatherData*)payload);
    }
}
```

### Strategy 2: Dependency Injection & Interfaces

Use C++ interfaces (pure virtual classes) to define contracts. This allows you to swap out modules (e.g., swapping `M5Display` for a `MockDisplay` when running unit tests on a PC).

```cpp
// Interface
class IDisplay {
public:
    virtual void drawWidget(Widget* w, int x, int y) = 0;
    virtual void partialRefresh() = 0;
};

// Implementation knows about hardware
class EInkDisplay : public IDisplay { ... };

// UI module only knows about the interface
class Dashboard {
    IDisplay* _display;
public:
    Dashboard(IDisplay* displayAdapter) : _display(displayAdapter) {}
};
```

### Strategy 3: Polymorphic Display Abstraction (E-Ink / TFT / LED)

To future-proof the hardware so the primary display can be effortlessly swapped between E-Ink, an LCD/TFT screen, or a low-resolution LED matrix without rewriting any UI components, we expand the Interface concept using deep polymorphism.

*   **Abstract Display Driver (`IDisplayController`)**: A pure virtual class standardizing drawing primitives (lines, text, buffers), power states, and screen flushing routines.
*   **Concrete Implementations**:
    *   `EInkDriver`: Translates generic draw commands into M5GFX 1-bit buffer updates. Specifically maps `flushBoundingBox()` to handle e-ink partial refresh (`epd_fastest`) or full clear (`epd_quality`) to prevent ghosting.
    *   `TFTDriver`: Extends the exact same interface but handles rapid SPI/DMA transfers for continuous high-refresh-rate redraws. It safely ignores e-ink ghosting and anti-aliasing constraints.
    *   `LEDMatrixDriver`: Overrides the interface for a low-resolution pixel grid, mapping drawing actions to simple geometric LEDs and ignoring complex vector fonts.
*   **Semantic Color Abstraction**: Because E-Ink is B&W/Grayscale and TFT is RGB565, UI code can no longer call hardcoded hardware colors like `TFT_RED` or `M5GFX_WHITE`. Instead, widgets use a semantic enum (`THEME_BACKGROUND`, `THEME_FOREGROUND`, `THEME_HIGHLIGHT`, `THEME_WARNING`). The polymorphic driver evaluates the enum and translates it to physical capabilities (e.g., `EInkDriver` translates `THEME_HIGHLIGHT` to `BLACK`, while `TFTDriver` translates it to a vivid RGB Orange).

```cpp
enum ThemeColor { THEME_BKG, THEME_FG, THEME_HIGHLIGHT, THEME_WARNING };

// Abstract Base Class enforces the contract
class IDisplayController {
public:
    virtual void init() = 0;
    virtual void clear(ThemeColor color) = 0;
    virtual void drawText(int x, int y, const char* text, ThemeColor color) = 0;
    
    // Unifies E-ink partial refresh with TFT DMA rectangle pushing
    virtual void flushBoundingBox(int x, int y, int w, int h) = 0; 
    
    virtual void sleep() = 0;
    virtual ~IDisplayController() = default;
};

// Drop-in Concrete Implementations
class EInkDriver : public IDisplayController { /* Implements slow 1-bit E-ink logic */ };
class TFTDriver : public IDisplayController { /* Implements fast 16-bit RGB TFT logic */ };
class LEDMatrixDriver : public IDisplayController { /* Implements addressable LED grid logic */ };

// Initialization via Factory / Dependency Injection isolates UI from Hardware
IDisplayController* activeDisplay = new EInkDriver(); // Can easily swap to new TFTDriver();
```

By decoupling the display logic this way, the `DisplayCore` and `Widget` engines simply pass X/Y coordinates and Semantic Colors to `activeDisplay->draw*(...)`, entirely unaware of the physical screen technology they are rendering to.

---

## 4. Re-Organizing the Codebase (Directory Structure)

To enforce these modular boundaries in PlatformIO, `lib/` should be refactored to separate interfaces, models, and implementations.

```text
lib/
├── Core/               # The Mediator, EventBus, and deep sleep lifecycle logic
├── Models/             # Pure state structs containing NO business logic
│   ├── WeatherData.h   # (RTC_DATA_ATTR compatible)
│   ├── SystemState.h
│   └── ConfigData.h
├── HAL/                # Hardware Abstraction
│   ├── HardwareRTC.cpp # Wraps esp_sleep_enable_ext0...
│   ├── BatteryMonitor.cpp
│   └── TouchDriver.cpp # Wraps GT911 
├── Services/           # The workers
│   ├── ApiClient.cpp   # Base HTTP abstractions
│   ├── OpenWeather.cpp
│   └── Provisioning.cpp
└── UI/                 # Presentation
    ├── DisplayCore.cpp # M5Gfx wrapper (implements IDisplay)
    ├── Widgets/        # Modular UI components (ClockWidget, GraphWidget)
    └── Pages/          # Screen layouts
```

---

## 5. State Management & `RTC_DATA_ATTR`

A major challenge in ESP32 deep-sleep firmwares is managing data that must survive reboots. Dispersing `RTC_DATA_ATTR` variables across 10 different files creates memory fragmentation and makes it impossible to serialize state or debug sleep crashes.

**The Modular Solution:**
Create a single, consolidated `SystemStore` containing all persistent state.

```cpp
// lib/Models/SystemState.h
struct SystemState {
    uint8_t sleepCycleCount;
    float lastBatteryVoltage;
    bool isNightModeActive;
    WeatherData currentWeather;
};

// Declared exactly ONCE in the core implementation
RTC_DATA_ATTR SystemState g_State;
```

All modules (UI, Network, AppController) receive a pointer/reference to `g_State` during their `begin()` or constructor phase.

---

## 6. Migration Path

Rewriting an embedded monolith is risky. The modularization must happen iteratively:

1. **Phase 1: Model Extraction.** Extract all structs, enums, and `RTC_DATA_ATTR` variables from existing `.cpp` files into a unified `lib/Models/` folder. Ensure the app still compiles.
2. **Phase 2: UI Detanglement.** Implement the `Widget` base class from the UI/UX Research. Move drawing logic out of `DisplayManager` into specific Widgets.
3. **Phase 3: Hardware Abstraction.** Replace raw `esp_sleep...` and `WiFi...` calls inside `AppController` with `HAL::Sleep`, `HAL::Radio`.
4. **Phase 4: Orchestration.** Introduce a simple Event/Message bus to break the cross-includes between singletons. Replace direct `Manager::getInstance().doThing()` calls with event publications.
