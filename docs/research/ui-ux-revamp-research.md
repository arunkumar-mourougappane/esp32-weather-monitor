# UI/UX Revamp Research & Mockups

## 1. Executive Summary

This document presents a research initiative to improve the user interface and user experience (UI/UX) of the M5Paper Weather Monitor. The primary goal is to address the high information density on the "Today" screen, seamlessly integrate the newly introduced "Hourly Forecast" and "Swipe-Up Detail Overlay" concepts, and leverage the unique characteristics of the e-ink display for maximum glanceability.

### UX Goals

- **Glanceability:** The most critical information (Time, Temp, Next Precipitation) must be legible from 3 meters away.
- **Progressive Disclosure:** Users should see core data on the main loop and use gestures (swipe-up) to access secondary environmental data (AQI, Pollen, UV).
- **Reduced Cognitive Load:** Prevent "data soup" by grouping related metrics logically (e.g., separating "Air Quality" from "Astronomical Data").

---

## 2. Information Architecture Restructuring

Currently, horizontal swiping transitions the user across distinct paradigms:`[ Dashboard <-> 10-Day <-> Settings ]`.

**Proposed Architecture:**

1. Horizontal swipes control the **Time Horizon**.
   `[ Settings ] <-> [ Today's Summary ] <-> [ 24h Hourly Forecast ] <-> [ Trends (Sparklines) ] <-> [ 10-Day View ]`
2. Vertical swipes control the **Detail Depth**.
   `[ Today's Summary ] <──(Swipe Up)──> [ Environmental Deep-Dive ]`

---

## 3. Redesigned UI Mockups

*ASCII scale used below: 1 character ≈ 10 px wide · 1 line ≈ 20 px tall.*
*Display is 540 × 960 px.*

### A. Page 1: Today Summary (Glanceable Hero View)

This page is stripped of dense grids to focus solely on what matters right now. Note the swipe up indicator at the bottom.

```text
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │                                       65%  [███░░ ▌] │
     │                                                      │
  20 │                   2:51 PM                            │ ← Time (FreeSansBold24pt x2)
     │                                                      │
 110 │                  Chicago, IL                         │ 
 160 │            Sunday, March 09 2026                     │ 
 200 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │                      [ Hero                          │
 280 │      32.5°C            Weather                       │ ← Massive Hero Section
     │                        Icon ]                        │
     │                                                      │
 390 │                 Partly Cloudy                        │
 445 │         H: 35.0°C   L: 21.0°C   Feels: 34.2°C        │ ← Primary stats
     │                                                      │
 520 ├─ Next 6 Hours ───────────────────────────────────────┤
     │                                                      │
     │   3 PM      4 PM      5 PM      6 PM      7 PM       │ ← Micro Hourly Strip
     │   [☀]       [⛅]       [☁]       [🌧]      [🌧]      │
     │  33°C      34°C      32°C      28°C      26°C      │
     │   0%        5%       20%       85%       90%       │
     │                                                      │
 780 ├──────────────────────────────────────────────────────┤
     │                                                      │
 865 ████████████ ⚠  Tornado Watch in Effect ████████████████ ← Alert Banner
     │                                                      │
 910 │                       ˄                              │
 930 │            Swipe Up for Deep-Dive Details            │ ← Affordance for Swipe Up
 950 │            ◉   ○   ○   ○   ○                         │ ← 5 dots for all pages
 960 └──────────────────────────────────────────────────────┘
```

### B. Swipe-Up Detail Overlay (Environmental Dashboard)

Triggered by a bottom-to-top swipe on the Today Summary page. This acts as a modal overlay drawn using `epd_fastest` sliding up from the bottom.

```text
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │   [███░░ ▌] 65%                           2:51 PM    │
     │                                                      │
  30 │                       ˅                              │
  50 │           Swipe Down to return to Today              │
  70 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │   Air Quality & Environment                          │
  90 │  ┌─────────────┐                                     │
     │  │   ╱── arc   │   AQI: 48 (Good)                    │ 
 130 │  │   /         │   Pollen: 12 (Grass)                │ 
     │  │  ╱  needle  │   UV Index: 4 (Moderate)            │
 200 │  │    ●   48   │   Visibility: 12 km                 │
     │  └─────────────┘                                     │
 240 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │   Wind & Pressure                                    │
 290 │  [ Wind Rose ]     Wind: NW 15 km/h                  │
     │  [   Icon    ]     Gusts: 24 km/h                    │
 360 │                    Pressure: 1012 hPa (Falling)      │
     │                                                      │
 400 ├──────────────────────────────────────────────────────┤
     │   Astronomical                                       │
     │  ┌──────────────────┐                                │
 450 │  │   dome arc  ───  │      Moon Phase                │
     │  │   ──── ☀ ──      │     [ Moon Icon ]              │
 500 │  │                  │      Waning Gibbous            │
     │  │ 6:42      19:53  │      Illumination: 78%         │
     │  └──────────────────┘                                │
     │                                                      │
 600 ├──────────────────────────────────────────────────────┤
     │                     Humidity: 62%                    │
 650 │                     Cloud Cover: 45%                 │
     │                                                      │
 960 └──────────────────────────────────────────────────────┘
```

### C. Page 2 (New): 24-Hour Hourly Forecast

Accessed via right-to-left swipe from the Today page. It solves the gap when users need more granular data than what's on the main screen, but don't want to browse the 10-day summary.

```text
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │   [███░░ ▌] 65%                           2:51 PM    │
     │                                                      │
 100 │  < Today                 Hourly             Trends > │ ← Navigation hints
 150 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │   Time   │ Temp  │ Precip │ Wind   │ Condition       │
 220 │  ────────┼───────┼────────┼────────┼──────────────── │
 260 │  08:00   │  22°  │   0%   │ NW 15  │ [☀] Sunny       │
 320 │  09:00   │  24°  │   0%   │ NW 16  │ [☀] Sunny       │
 380 │  10:00   │  26°  │  10%   │ NW 18  │ [⛅] P. Cloudy  │
 440 │  11:00   │  28°  │  30%   │ N  20  │ [☁] Cloudy      │
 500 │  12:00   │  29°  │  80%   │ N  25  │ [🌧] Rain       │
 560 │  13:00   │  28°  │  95%   │ NE 28  │ [⛈] T. Storm    │
 620 │  14:00   │  26°  │  60%   │ NE 20  │ [🌧] Rain       │
 680 │  15:00   │  25°  │  20%   │  E 15  │ [☁] Cloudy      │
 740 │  16:00   │  24°  │   0%   │  E 12  │ [⛅] P. Cloudy  │
     │                                                      │
     │                 (Swipe to scroll)                    │
     │                                                      │
 940 │            ○   ◉   ○   ○   ○                         │ ← Hourly is Page 2
 960 └──────────────────────────────────────────────────────┘
```

### D. Page 3 (New): Trend Sparklines

Accessed via right-to-left swipe from the Hourly Forecast page. Provides visual sparklines and graphs for a wider range of environmental parameters over the upcoming timeframe.

```text
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │   [███░░ ▌] 65%                           2:51 PM    │
     │                                                      │
 100 │  < Hourly                Trends             10-Day > │ ← Navigation hints
 150 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │              Temperature & Dew Point                 │
 220 │       │ ╱─────╲     ╱╲      ╱╲      ───────          │
     │       │          ───   ────    ────                  │
 300 │       │  ·····················  ·················    │
     │                                                      │
 340 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │              Humidity & Cloud Cover (%)              │
 420 │ 100 │                                                │
     │     │   ██                                           │
 500 │     │   ██    ██              ██                     │
     │                                                      │
 540 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │              Wind Speed & Gusts (km/h)               │
 600 │  40 │      ╱╲                                        │
     │     │ ╱───/  \  ╱╲  ────                             │
 680 │     │/        \/  \/                                 │
     │                                                      │
 720 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │              Barometric Pressure (hPa)               │
 800 │ 1015│ ───────╲                                       │
     │     │         \╱─────╲                   ╱──         │
 880 │     │                 \─────────────────/            │
     │                                                      │
 940 │            ○   ○   ◉   ○   ○                         │ ← Trends is Page 3
 960 └──────────────────────────────────────────────────────┘
```

## 4. Implementation Guidelines

To implement this UI paradigm successfully in the current embedded C++ stack:

### A. Partial Refresh Strategy

The Swipe-Up detail screen will require rendering a large block of new text. To avoid the highly disruptive full E-Ink flash (`epd_quality`), we should utilize the `epd_fastest` mode with bounding boxes. We can slide the overlay upwards in 3 or 4 fast steps, or just pop it onto the screen with a clean black-on-white redraw over the bottom 80% of the screen.

### B. Font Choices

Ensure `FreeSansBold24pt` is used sparingly (Hero temps and Time) while keeping data-heavy tables like the Hourly View strictly to `FreeSans12pt` to guarantee column alignment using the existing bounding box methods (`drawCentreString`, `drawString`).

### C. Gestures & Input

Updating `InputManager` to catch **Swipe Up** and **Swipe Down**:

- `delta-Y >= 50 px` → Next Detail View
- `delta-Y <= -50 px` → Prev Detail View
Currently, the touch array logic likely only captures delta-X for the main dashboard loop.

### D. Refactoring the DisplayManager Pages

- Add `PAGE_HOURLY` and `PAGE_TRENDS` to the enum (`PAGE_TODAY`, `PAGE_HOURLY`, `PAGE_TRENDS`, `PAGE_10_DAY`, `PAGE_SETTINGS`).
- Break down `drawTodayPage()` into `drawTodayHero()` and `drawTodayDetailsOverlay()`.
- Ensure `RTC_DATA_ATTR` buffers are sized correctly to cache at least 24 elements of the hourly array for fast rendering without network fetches.

### E. Page 5: Settings & Diagnostics

A redesigned, cleaner settings page separating actionable controls from passive diagnostic data. Interaction feedback is provided via fast-refresh inversion to avoid user confusion.

```text
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │   [███░░ ▌] 65%                           2:51 PM    │
     │                                                      │
     │                 Device Settings                      │
 150 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐│
 220 │  │              │  │              │  │              ││
     │  │   [ Sync ]   │  │   [ WiFi ]   │  │  [ Sleep ]   ││
     │  │    Now       │  │    Setup     │  │   Device     ││
 320 │  │              │  │              │  │              ││
     │  └──────────────┘  └──────────────┘  └──────────────┘│
 360 │    Last: 2:30 PM     192.168.1.50      Interval: 30m │
     │                                                      │
 440 ├─ System Diagnostics ─────────────────────────────────┤
     │                                                      │
 500 │  Battery Health:     Good (3.9V)                     │
 540 │  Est. Runtime:       ~45 hours                       │
 580 │  Network Status:     [✓] Connected (RSSI: -65dBm)    │
 620 │  Last IP Address:    192.168.1.50                    │
 660 │  Firmware Version:   v3.1.0                          │
 700 │  Sync Interval:      Adaptive (Next: 3:00 PM)        │
     │                                                      │
 940 │            ○   ○   ○   ○   ◉                         │ ← Settings is Page 5
 960 └──────────────────────────────────────────────────────┘
```

## 5. Interaction & Navigation Research

### A. Maximizing Screen Real Estate (Gesture vs. UI Buttons)

To prevent navigation elements from consuming valuable data space, we must optimize how navigation is conveyed without losing data presentation:

- **Remove Textual Headers:** By eliminating the textual navigation headers (e.g., `< Today    Hourly >`) from the sub-pages, we reclaim roughly 50px of vertical space for charts and data.
- **Global Swipes & Unified Pagination:** Limit permanent navigation UI to an ultra-thin 20px strip at the very bottom containing only the 5 pagination dots (`◉ ○ ○ ○ ○`). Since the touch panel covers the display, horizontal screen swipes anywhere on the Y-axis will trigger page changes.
- **Invisible Touch Zones vs Visual Clutter:** Standard touch gestures (swiping left/right anywhere, or swiping up on the Home screen) eliminate the need for persistent on-screen buttons, maximizing the pristine, print-like aesthetic of the e-ink display.

### B. Visual Feedback on E-Ink (Avoiding Lag)

Capacitive touch on an e-ink display often feels unresponsive and sluggish because drawing the next state takes time, leading to users double-tapping and creating race conditions.

- **Button Tap Inversion:** When a user taps a setting button (e.g., "Sync Now"), the system should immediately execute a partial refresh (`epd_fastest`) that simply inverts the bounding box of that button (black background, white text). This provides instant (<150ms) tactile confirmation before the system starts the heavy, blocking tasks (like WiFi connection or deep-sleep prep).
- **Toast Notifications:** Instead of switching to dedicated "Success/Error" full screens, use small inverted "Toast" overlays centered at the bottom of the screen to show ephemeral states (e.g., "Sync Queued...", "WiFi AP Started"). These can slide up or pop-in using bounding boxes and clear themselves without disrupting the main page background.

### C. Modals and Blocking UIs

When the system needs to prevent user interaction (e.g., during a critical firmware update, manual WiFi sync, or writing to NVS), standard UI rendering paradigms must be adapted for e-ink:

- **Full-Screen "Glass" Modals:** Drawing a semi-transparent dark overlay (e.g., repeating a checkerboard alternating pixel pattern) across the entire 540x960 screen using `epd_fastest`, and placing a solid white dialog box in the center. This visually communicates to the user that the background interface is "frozen" or "blocked."
- **Spinner Alternatives:** E-ink cannot fluidly display a spinning wheel. Instead, during a modal blocking event, utilize a stepped progress bar or a sequence of solid filled squares (e.g., `[■] [ ] [ ]` -> `[■] [■] [ ]`) updated strictly via bounding-box partial refreshes to show ongoing background activity.
- **Modal Dismissal:** Once the blocking task completes, the entire screen should undergo a single `epd_quality` (full black-to-white flash) refresh to guarantee that any ghosting introduced by the modal or the partial refresh loaders is wiped clean before returning to the standard dashboard.

---

## 6. Component Modularity & Project Restructuring

Currently, `DisplayManager.cpp` handles all drawing logic, which creates a monolithic structure that is difficult to maintain and restricts UI flexibility.

### A. UI Element Modularity (Widget Concept)

To allow easy movement of UI elements (e.g., deciding to move the Battery icon from the top right to the bottom left without rewriting coordinates across 5 functions), the UI should transition to a component-based architecture:

- **Abstract `Widget` Class:** Create a base class `class UIWidget` with `draw(x, y)` and `getBoundingBox()` methods.
- **Independent Canvas Sprites:** The M5GFX library supports multiple sprites. Instead of drawing directly to one massive global sprite, smaller widgets (like `BatteryWidget`, `ClockWidget`, `WeatherIconWidget`) can render to their own small memory sprites and then push to the main display sprite.
- **Relative Positioning:** By passing an `x, y` coordinate to a modular `draw(x, y)` method, the internal drawing functions of the widget act relatively. This strictly isolates the drawing logic from the page layout logic.

### B. Project Structure Refactoring for Hardware Optimization

To better leverage the ESP32 hardware and M5Paper's PSRAM (which has 4MB of pseudo-static RAM that goes largely unused if not explicitly targeted):

- **Isolate Display Logic:** Move from a single `DisplayManager` to:
  - `lib/Display/Core`: Handles M5GFX init, E-Ink refresh parameters, and global sprites.
  - `lib/Display/Widgets`: Contains the modular component classes (e.g., `BatteryGauge.cpp`, `SparklineGraph.cpp`).
  - `lib/Display/Pages`: Contains the layout arrangement for specific screens (e.g., `TodayPage.cpp`, `SettingsPage.cpp`).
- **Leverage PSRAM for UI Sprites:** Using `sprite.createSprite(...)` normally allocates from internal SRAM, which is limited. Modifying the project setup to ensure large UI canvas buffers explicitly allocate in PSRAM (`ps_malloc`) will prevent heap fragmentation and out-of-memory crashes when building complex, multi-page UIs and fast-refresh overlay buffers.
- **Struct / Data Separation:** Disconnect the fetching mechanism (`WeatherService`) completely from the UI drawing. The UI should only ever read from a globally accessible `struct RTC_DATA_ATTR WeatherDataCache` so drawing doesn't trigger API logic or wait on networking strings.

---

## 7. Future Enhancements & Advanced E-Ink Concepts

### A. Dark Mode (Inverted Display)

Because E-ink displays look striking when inverted (black background, white text), a user-selectable "Dark Mode" could be introduced.

- **Implementation:** The `M5GFX` library supports color inversion. This would require updating the base `Widget` class to accept a `theme` parameter so that borders, icons, and text bounds are drawn using `WHITE` on `BLACK` instead of `BLACK` on `WHITE`.
- **Advantage:** Visually distinct, very high contrast, and reduces the visual glare of a large 4.7" white panel in a dark room.

### B. Ghosting Mitigation Strategy

E-ink screens inherently accumulate "ghosting" (faint remnants of previous screens) after multiple fast partial refreshes (`epd_fastest`).

- **Automated Scrubbing:** Implement a "scrub" routine that performs a full blanking cycle (flashing black, then white) automatically at midnight, or after a specific threshold (e.g., every 50 partial refreshes).
- **Graceful Swipes:** If users are swiping through many pages rapidly, wait until swipe activity pauses for >2 seconds, and then perform a single `epd_quality` refresh on the final resting page to clean up artifacting left behind by the quick swipe transitions.

### C. Typography & Font Anti-Aliasing

The 1-bit display depth (black/white only) means standard greyscale anti-aliasing results in muddy, dithering artifacts.

- **Strict Hard-Edged Fonts:** Ensure all TrueType fonts are configured with rendering parameters that disable anti-aliasing, enforcing a sharp threshold.
- **Pre-rendered Bitmaps:** For highly stylized or massive hero numbers (like the main temperature), consider dropping TrueType generation entirely in favor of using pre-rendered, hand-tuned bitmap arrays (created via toolchains like `fontconvert`). This ensures perfectly sharp diagonal lines.

### D. User-Customizable Dashboard

By migrating to the Widget-based modular system outlined in Section 6, the device gains the potential for extreme customization.

- **The Web Portal:** The captive portal's web interface could be upgraded to include a drag-and-drop dashboard builder. Users could select which widgets (e.g., "AQI Dial", "Sun Arc", "Hourly Strip") they want rendered on their 'Today' page, passing an array of widget IDs down to the ESP32 to be stored in NVS. The `TodayPage.cpp` layout engine would then iterate over this user-defined array to build the screen dynamically.

---

## 8. Web Provisioning & Captive Portal Modernization

The captive portal (`192.168.4.1`) is the core onboarding experience for setting up Wi-Fi, APIs, and sync intervals. Currently, embedded portals often rely on basic HTML forms that require full page reloads to submit data to the ESP32, resulting in a clunky, broken experience on modern smartphones.

### A. Single Page Application (SPA) Architecture

To solve the "reloading drops the AP connection" issue common on iOS/Android captive portals:

- **Fetch API (AJAX) Submissions:** Move away from standard `<form action="/save" method="POST">` tags. Instead, use vanilla JavaScript `fetch()` to POST JSON payloads. This prevents the captive browser from trying to navigate away from the current page while the ESP32 is busy validating and saving to NVS.
- **Client-Side Routing:** Use JS to show/hide `<section>` blocks (e.g., "Wi-Fi Config", "API Keys", "Device Settings") without requesting new HTML pages from the ESP32, drastically reducing the micro-controller's memory overhead and making the UI feel native and instant.

### B. JavaScript Driven UX & Validation

Offload processing overhead from the ESP32 directly to the user's smartphone processor:

- **Instant Input Validation:** Instead of sending an API key to the ESP32 and waiting for a C++ error response if it's too short, use JS to validate string lengths, regex patterns for IP addresses/Webhooks, and number boundaries (e.g., checking if the sync interval is between 10m and 12h) *before* the submit button enables.
- **Dynamic Network Scanning:** Add a "Scan Networks" button that hits an ESP32 `/scan` endpoint returning a JSON array of SSIDs. JS can then render these into a clean dropdown menu, preventing users from typing typos into plain text fields.
- **Abstracting Geolocation Details:** Users rarely know their latitude and longitude coordinates. Instead of making them copy/paste them from Google Maps:
  - *Browser Geolocation API:* First, attempt an auto-fill using JS `navigator.geolocation.getCurrentPosition()`.
  - *City Search Abstraction:* Allow the user to simply type `"Chicago, IL"` into a search bar, and have JavaScript ping a free reverse-geocoding API (e.g., OpenStreetMap's Nominatim or the OpenWeatherMap Geocoding API) from their phone to invisibly fetch the exact coordinates and save those to the ESP32.
- **Password Toggling:** Simple JS interactions like a "Show Password" eye-icon for the SSIDs greatly reduce onboarding friction.
- **Visual Feedback:** Implement JS loading spinners over the "Save" buttons and disable inputs during save operations so users don't multi-click while the ESP encrypts and stores the data.

### C. Modern Styling (CSS Optimization)

To make the portal feel like an extension of a premium hardware product:

- **CSS Variables:** Utilize standard CSS custom properties (`:root { --primary: #000; }`) to create a clean, minimalist black-and-white theme mirroring the physical E-ink device's aesthetic.
- **Flexbox/Grid Layouts:** Ditch standard block flowing for native CSS Flexbox, ensuring the layout scales perfectly regardless of whether the user configures the device from a towering iPad or a narrow Android phone screen.
- **GZIP Compression:** To save Flash/SRAM space on the ESP32, map the entire finalized HTML+CSS+JS file into a single string macro, compress it using GZIP via a Python pre-upload script, and have the ESP32's WebServer serve the `.gz` blob with the `Content-Encoding: gzip` header. This will reduce a 20KB modern SPA portal down to ~4KB in device memory.

### D. Captive Portal UI Mockup

*A clean, mobile-first Web interface mimicking the e-ink aesthetic (black, white, and grey). Using CSS Grid and Flexbox, this layout will responsively adapt its width from a narrow smartphone screen gracefully up to a full desktop monitor without breaking.*

#### Mobile Phone View (Narrow)
```text
┌───────────────────────────────────────────┐
│  M5Paper Weather Setup                [≡] │ ← Sticky Header with hamburger menu
├───────────────────────────────────────────┤
│                                           │
│  Wi-Fi Networks                     [⟳]   │ ← Scan Button
│  ───────────────────────────────────────  │
│                                           │
│  [ Home_Network_5G         ▼ ]  [- 62dB]  │ ← Scanned dropdown
│                                           │
│  ┌─────────────────────────────────────┐  │
│  │ Password                      [ 👁 ] │  │ ← Password input with toggle
│  └─────────────────────────────────────┘  │
│                                           │
│  + Add Another Network (Up to 5)          │
│                                           │
│  API Configuration                        │
│  ───────────────────────────────────────  │
│                                           │
│  Location Setup                           │
│  ┌─────────────────────────────────────┐  │
│  │ 🔍 Chicago, IL                  [◎] │  │ ← Search input with Auto-locate button
│  └─────────────────────────────────────┘  │
│  ✓ Saved: (41.8781° N, 87.6298° W)        │ ← JS Geocoding feedback (hidden from user input)
│                                           │
│  OpenWeatherMap API Key                   │
│  ┌─────────────────────────────────────┐  │
│  │ ••••••••••••••••••••••••••••••••••• │  │ 
│  └─────────────────────────────────────┘  │
│  ✓ Key format valid (32 chars)            │ ← JS Validation feedback
│                                           │
│  Device Settings                          │
│  ───────────────────────────────────────  │
│                                           │
│  Sync Interval (Minutes)                  │
│  [  30  ] ───◉────────────────────────    │ ← HTML Range Slider (10 to 720)
│                                           │
│  Use Dark Mode?             ( ) No (◉) Yes│ ← Radio buttons for new theme feature
│                                           │
│  [          Save Configuration         ]  │ ← Submit button (disables during save)
│  [        Reboot Device (Apply)        ]  │ ← Reboot button (only visible after save)
│  [        Factory Reset (Erase)        ]  │ ← Button to wipe NVS and reboot
│  M5Paper Weather V3.1.0 • Battery: 65%    │ ← Device info fetched via /status endpoint
└───────────────────────────────────────────┘
```

#### Tablet/Desktop View (Wide - 2 Column Layout)
*When the viewport exceeds 768px (e.g., standard tablet portrait), CSS `@media` queries shift the layout into two columns, utilizing the extra horizontal space.*

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│  M5Paper Weather Setup                                                  [≡] │ 
├─────────────────────────────────────────────────────────────────────────────┤
│                                     │                                       │
│  Wi-Fi Networks               [⟳]  │  API Configuration                    │
│  ─────────────────────────────────  │  ───────────────────────────────────  │
│                                     │                                       │
│  [ Home_Network_5G    ▼ ] [- 62dB]  │  Location Setup                       │
│                                     │  ┌─────────────────────────────────┐  │
│  ┌───────────────────────────────┐  │  │ 🔍 Chicago, IL              [◎] │  │ 
│  │ Password               [ 👁 ] │  │  └─────────────────────────────────┘  │
│  └───────────────────────────────┘  │  ✓ Saved: (41.8781° N, 87.6298° W)    │
│                                     │                                       │
│  + Add Another Network (Up to 5)    │  OpenWeatherMap API Key               │
│                                     │  ┌─────────────────────────────────┐  │
│                                     │  │ ••••••••••••••••••••••••••••••• │  │ 
│                                     │  └─────────────────────────────────┘  │
│  Device Settings                    │  ✓ Key format valid (32 chars)        │
│  ─────────────────────────────────  │                                       │
│                                     │  Action Panel                         │
│  Sync Interval (Minutes)            │  ───────────────────────────────────  │
│  [  30  ] ───◉────────────────────  │                                       │
│                                     │  [       Save Configuration        ]  │
│  Use Dark Mode?       ( ) No (◉) Yes│  [     Reboot Device (Apply)       ]  │
│                                     │  [     Factory Reset (Erase)       ]  │
│                                     │                                       │
├─────────────────────────────────────┴───────────────────────────────────────┤
│  M5Paper Weather Monitor V3.1.0 • Battery: 65% • Uptime: 01:14:22           │ 
└─────────────────────────────────────────────────────────────────────────────┘
```
