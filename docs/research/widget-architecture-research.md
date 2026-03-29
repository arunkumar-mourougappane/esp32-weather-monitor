# UI Widget Architecture & Core Program Structure

## 1. Objective and Motivation

In a monolithic embedded UI, drawing logic is often hardcoded with absolute coordinates inside massive `drawPage()` functions. This makes the code brittle: moving a single temperature readout requires changing X/Y values in multiple places, and re-using an element (like a battery icon) across different screens results in duplicated code.

By adopting a **Component-Based UI Architecture (Widget Pattern)**, we encapsulate the layout, state, and drawing instructions of individual UI elements. This allows us to separate functionality, establish a reusable core structure, and dramatically reduce the complexity of screen layouts.

---

## 2. The Abstract `Widget` Base Class

Every visual element on the screen must inherit from a common interface. This enables polymorphism, allowing a `Page` layout to simply iterate through a list of generic `Widget*` pointers without knowing what they specifically draw.

### The Contract (`Widget.h`)

```cpp
#ifndef WIDGET_H
#define WIDGET_H

#include "IDisplayController.h"

class Widget {
protected:
    int16_t _x, _y;         // Top-left origin
    uint16_t _w, _h;        // Bounding box dimensions
    bool _isDirty;          // State flag for E-ink partial redraws
    IDisplayController* _gfx; // Pointer to the hardware abstraction

public:
    Widget(IDisplayController* gfx, int16_t x, int16_t y, uint16_t w, uint16_t h) 
        : _gfx(gfx), _x(x), _y(y), _w(w), _h(h), _isDirty(true) {}
        
    virtual ~Widget() = default;

    // Forces a full redraw of the widget
    virtual void draw() = 0;

    // Checks if internal state changed before pushing to the screen
    bool isDirty() const { return _isDirty; }
    void clearDirty() { _isDirty = false; }
    void markDirty() { _isDirty = true; }

    // Bounding box getters for E-ink partial refresh (epd_fastest) targeting
    int16_t getX() const { return _x; }
    int16_t getY() const { return _y; }
    uint16_t getWidth() const { return _w; }
    uint16_t getHeight() const { return _h; }
};

#endif
```

---

## 3. Concrete Widget Examples (Separation of Functionality)

Each subclass encapsulates only the logic required for its specific domain.

### A. `ClockWidget`

Responsible solely for rendering time.

```cpp
class ClockWidget : public Widget {
private:
    struct tm _currentTime;
public:
    ClockWidget(IDisplayController* gfx, int16_t x, int16_t y) 
        : Widget(gfx, x, y, 200, 80) {}

    // Receive injected data (Data Binding)
    void updateTime(struct tm newTime) {
        if (_currentTime.tm_min != newTime.tm_min) {
            _currentTime = newTime;
            markDirty(); // Only redraw if the minute actually changed!
        }
    }

    void draw() override {
        // Draw using relative coordinates to its own _x, _y bounding box
        _gfx->setFont(BigFont);
        _gfx->drawString(String(_currentTime.tm_hour) + ":" + String(_currentTime.tm_min), _x, _y);
        clearDirty();
    }
};
```

### B. `BatteryWidget`

Responsible for interpreting ADC voltage into a 5-bar iconography.

```cpp
class BatteryWidget : public Widget {
private:
    uint8_t _percentage;
public:
    // ... constructor ...
    void updateBattery(uint8_t percentage) {
        if (_percentage != percentage) {
            _percentage = percentage;
            markDirty();
        }
    }
    void draw() override {
        // Logic to draw [███░░] based on _percentage at (_x, _y)
    }
};
```

---

## 4. Core Program Structure (Pages as Containers)

A **Page** is essentially a specialized collection (or scene) that manages the lifecycle and layout of multiple Widgets.

### The `Page` Engine

```cpp
#include <vector>

class Page {
protected:
    std::vector<Widget*> _widgets;
    IDisplayController* _gfx;

public:
    Page(IDisplayController* gfx) : _gfx(gfx) {}
    virtual ~Page() {
        for (auto w : _widgets) {
            delete w;
        }
    }

    void addWidget(Widget* w) {
        _widgets.push_back(w);
    }

    // The core render loop
    void render(bool forceFullRefresh = false) {
        for (auto w : _widgets) {
            if (w->isDirty() || forceFullRefresh) {
                w->draw();
                // Optionally command the driver to partially refresh this bounding box
                if (!forceFullRefresh) {
                    _gfx->flushBoundingBox(w->getX(), w->getY(), w->getWidth(), w->getHeight());
                }
            }
        }
    }
};
```

### Page Implementation Concept

Inside the actual UI code, building a screen becomes entirely declarative, making it incredibly clean.

```cpp
// TodayPage.cpp
class TodayPage : public Page {
    ClockWidget* clock;
    BatteryWidget* battery;
    WeatherHeroWidget* weatherHero;

public:
    TodayPage(IDisplayController* gfx) : Page(gfx) {
        // Layout is defined exactly once in the constructor
        clock = new ClockWidget(gfx, 10, 10);
        battery = new BatteryWidget(gfx, 450, 10);
        weatherHero = new WeatherHeroWidget(gfx, 10, 100);
        
        addWidget(clock);
        addWidget(battery);
        addWidget(weatherHero);
    }

    // Pass the state structurally to the widgets
    void syncData(const SystemState& state) {
        clock->updateTime(state.time);
        battery->updateBattery(state.batteryPercent);
        weatherHero->updateWeather(state.currentWeather);
    }
};
```

---

## 5. Advanced E-ink Layout Strategies

### A. Relative & Local Coordinate Systems

Instead of passing absolute global screen coordinates to M5GFX primitives, the `IDisplayController` can introduce an **origin offset**.
If a widget's origin is `(X: 100, Y: 200)`, when the widget issues `_gfx->drawLine(0, 0, 10, 10)`, the HAL driver automatically translates that to `(100, 200) -> (110, 210)`. This allows you to drag a widget completely across the screen without altering a single line of its internal drawing code.

### B. Sprite-Backed Widgets (PSRAM Optimization)

For complex widgets (like a `SparklineWidget`), drawing directly to the screen via `epd_fastest` primitives can sometimes cause ghosting.
Instead, the Widget can instantiate its own M5GFX 1-bit `Sprite` allocated in **PSRAM**.

1. The widget clears its internal RAM Sprite.
2. Draws vector lines to the Sprite.
3. Pushes the entire completed Sprite to the physical screen's bounding box in a single continuous DMA/I2S transaction.

## 6. Summary of Structural Flow

1. **The App Controller** wakes up and builds the `SystemState` struct.
2. **The App Controller** instantiates `TodayPage` (which internally builds the generic UI `Widgets`).
3. **The App Controller** calls `todayPage.syncData(SystemState)`.
4. Internally, widgets check if their newly injected data differs from their old data. If so, they set their `_isDirty` flag to `true`.
5. **The App Controller** calls `todayPage.render()`.
6. Only the components flagged as `_isDirty` are instructed to `draw()`, pushing clean, isolated bounding-box updates to the E-ink display layer.
