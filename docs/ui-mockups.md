# UI Mockups — M5Paper Weather Monitor

## Display Specifications

| Property | Value |
|---|---|
| Panel | 4.7" e-ink (electrophoretic) |
| Resolution | 540 × 960 px, portrait |
| Colour depth | 1-bit (black / white only) |
| Refresh modes | `epd_quality` — full clear + render; `epd_fastest` — partial region |
| Canvas | Off-screen M5GFX sprite flushed in a single SPI burst via `pushSprite(0,0)` |

**ASCII scale used in mockups below:** 1 character ≈ 10 px wide · 1 line ≈ 20 px tall.  
Approximate Y-pixel positions are annotated on the left margin of each diagram.

---

## Navigation Model

Three pages cycle via horizontal swipe on the touch panel:

```
  ─── swipe LEFT ──►
  Dashboard  ←→  Forecast  ←→  Settings
  ◄── swipe RIGHT ───
```

| Input | Threshold | Effect |
|-------|-----------|--------|
| Swipe left | delta-X ≥ 30 px (fires mid-drag) | Next page |
| Swipe right | delta-X ≤ −30 px (fires mid-drag) | Previous page |
| Tap | Release without crossing 30 px | Settings column selection |
| G38 rocker (EXT0) | GPIO 38 pulled low | Wake from deep sleep → timer-fetch path |

**Pagination dots** (Y 940, X 246 / 270 / 294 / 318): ◉ = active page, ○ = inactive page. Active page name label drawn above the dots at Y 930 (BC_DATUM, FreeSans9pt).

---

## Page 1 — Dashboard (Today)

Full `epd_quality` redraw after each weather fetch. On interactive wakeup,
every minute-tick updates only the **Y 0–95 clock strip** using `epd_fastest`
via `updateClockOnly()`.

```
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │                                                      │
  15 │                                       65%  [███░░ ▌] │ ← battery (X=485, 40×20 rect + nub)
  20 │                   2:51 PM                            │ ← time FreeSansBold24pt ×2, TC_DATUM
     │                                              NTP!    │ ← NTP! badge at (X=496,Y=22) — only
     │                                                      │   when _ntpFailed (NTP sync timed out)
 110 │                  Chicago, IL                         │ ← city FreeSans24pt ×1
 160 │            Sunday, March 09 2026                     │ ← date FreeSans18pt
 200 ├──────────────────────────────────────────────────────┤ ← full-width rule
     │                                                      │
 245 │                             28.5°C                   │ ← temp at (240,245) FreeSansBold24pt×1.5
     │  [weather icon]                                      │ ← icon: cx=140, cy=265, r=40
 265 │  (condition-matched                                  │
     │   vector; see icon table)                            │
 330 │                 Partly Cloudy                        │ ← condition FreeSans24pt, TC_DATUM
     │                                                      │
 390 │  Feels: 26.1°C          │  NW 15 km/h  [wind rose]   │ ← details row 1 (split X=270); compass prefix
 430 │  Humidity: 62%          │  Clouds: 45%               │ ← details row 2 (full label names)
 470 │  UV Index: 4            │  Visibility: 12 km         │ ← details row 3  FreeSans12pt
     │                                                      │
     │  ┌─────────────┐        │  ┌──────────────────┐      │
     │  │   ╱── arc   │        │  │   dome arc  ───  │      │
 570 │  │   /         │  AQI   │  │   ──── ☀ ──      │ Sun  │ ← AQI gauge cx=135 / Sun arc cx=405
     │  │  ╱  needle  │        │  │                  │      │   r=45 half-arc for both widgets
     │  │    ●   48   │        │  │                  │      │
 608 │  │    AQI      │ 6:42      │        19:53     │      │ ← sunrise MR_DATUM / sunset ML_DATUM
     │  └─────────────┘           └──────────────────┘      │   flanking sunCX=405, Y=608
 635 ├──────────────────────────────────────────────────────┤ ← rule
 655 │                      Tomorrow                        │ ← FreeSans18pt, TC_DATUM
     │                                                      │
 730 │                  [weather icon]                       │ ← cx=270, cy=730, r=20
 800 │                   Mostly Cloudy                       │ ← FreeSans18pt, TC_DATUM (below icon bottom 786)
 835 │         H: 30°C   L: 21°C   Precip: 30%               │ ← FreeSans12pt, TC_DATUM
     │                                                      │
 855 ████████████ ⚠  Tornado Watch in Effect ████████████████ ← inverted 32px strip, white text
 887 │                                                      │   FreeSans9pt, MC_DATUM; truncated @52+…
 940 │                    ◉  ○  ○                           │ ← pagination Y=940 (X=246/270/294)
 955 │                                     Updated: 14:35   │ ← FreeSansBold9pt, BR_DATUM (kWidth-15)
 960 └──────────────────────────────────────────────────────┘
```

### Dashboard Element Reference

| Y (px) | X (px) | Element | Font / Notes |
|-------:|-------:|---------|--------------|
| 15 | 485 | Battery: 40×20 outer rect + 4×10 nub + fill | Inline `%` MR_DATUM, X=480 |
| 20 | kWidth/2 | Time `"2:51 PM"` | FreeSansBold24pt ×2, TC_DATUM |
| 22 | kWidth−44 | `"NTP!"` badge | Default font ×1; only when `_ntpFailed` |
| 110 | kWidth/2 | City / State string | FreeSans24pt ×1 |
| 160 | kWidth/2 | Date `"Sunday, March 09 2026"` | FreeSans18pt |
| 200 | 20–520 | Horizontal rule | 1 px |
| 245 | 240 | Temperature `"28.5°C"` | FreeSansBold24pt ×1.5, TL_DATUM |
| 265 | 140 | Weather icon (condition-matched, r=40) | See icon vocabulary table |
| 330 | kWidth/2 | Condition string | FreeSans24pt |
| 390 | 40 / 290 | Feels Like / Wind + direction (8-point compass) | FreeSans12pt |
| 430 | 40 / 290 | Humidity / Cloud cover | FreeSans12pt |
| 470 | 40 / 290 | UV index / Visibility | FreeSans12pt |
| 570 | cx=135 | AQI half-arc gauge (r=45) + filled-triangle needle + pivot (r=6) | FreeSansBold9pt labels |
| 570 | cx=405 | Sun arc dome (r=45, r−4 thick) + sun dot (daytime) or crescent (night) | FreeSansBold9pt `"Sun"` |
| 608 | sunCX−48 / sunCX+48 | Sunrise / sunset times | FreeSansBold9pt, MR / ML datum |
| 635 | 20–520 | Horizontal rule | 1 px |
| 655 | kWidth/2 | `"Tomorrow"` label | FreeSans18pt, TC_DATUM |
| 730 | kWidth/2 | Tomorrow icon (r=20) | Condition-matched vector |
| 800 | kWidth/2 | Tomorrow condition | FreeSans18pt, TC_DATUM; shifted below icon bottom (730+56+14=800) |
| 835 | kWidth/2 | Tomorrow `"H: xx°C   L: xx°C   Precip: xx%"` | FreeSans12pt, TC_DATUM |
| 855–887 | 0–540 | Alert banner — inverted black rect + white text | FreeSans9pt, MC_DATUM; shown when `data.hasAlert` |
| 940 | 246 / 270 / 294 / 318 | Pagination dots (r=6 filled / r=5+4 hollow ring) | Active = filled; active page name at Y=930 BC_DATUM |
| 955 | kWidth−15 | `"Updated: HH:MM"` | FreeSansBold9pt, BR_DATUM |

---

## Page 2 — 10-Day Forecast

Temperature sparklines and precipitation bars appear at the top as always-visible charts.
Below the divider (Y 405), three scrollable day-cards sit in 180 px columns.
Swipe left/right increments / decrements `forecastOffset` (0–7); cards rebuild via `epd_fastest`.

```
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │                                       65%  [███░░ ▌] │ ← battery
     │                                                      │
  95 │  35°  │              Temperature         ══Hi  ─Lo   │ ← title TC_DATUM; legend: thick=Hi, thin=Lo
     │       │                                              │
 120 │       │ ╱─────╲     ╱╲      ╱╲      ───────          │ ← Hi temp line: thick 5-px polyline
     │       │          ───   ────    ────                  │
     │       │  ·····················  ·················    │ ← Lo temp line: thin 1-px polyline
 180 │       └────────────────────────────────── (x-axis)   │   axis at Y=yOff+chartH=180
     │                                                      │   padding=60, chartW=420, chartH=60
 270 │                    Rain Chance (%)                   │ ← title Y=yOff−20=270
     │       │                                              │
 290 │ 100   │                                              │ ← bar chart top
     │       │   ██                                         │
     │       │   ██    ██              ██                   │ ← filled bars = precipChance
     │       │   ██    ██   ██   ██    ██    ██   ██   ██   │   barW 6–28 px, centred at each day X
   0 │       └────────────────────────────────── (x-axis)   │   axis at Y=290+chartH=340
 340 │                                                      │
 405 ├──────────────────────────────────────────────────────┤ ← rule
     │          │              │                            │ ← column dividers at X=180, X=360
 418 │  Mon 03  │   Tue 04     │   Wed 05                   │ ← day label FreeSans12pt, TC_DATUM
     │          │              │                            │
 476 │ [☁ icon] │  [☀ icon]    │  [🌧 icon]                 │ ← icon cx per column, cy=476, r=22
 522 │ P.Cloudy │    Clear     │   Showers                  │ ← condition ≤12 chars, FreeSans9pt
 556 │ H:29 L:18│  H:32 L:21  │  H:24 L:17                 │ ← H/L FreeSans9pt, TC_DATUM
 581 │ ──[████]─│   ──[██]─── │    [███]───                 │ ← 100×7 temp range bar, X-proportional
 602 │ Precip: 30%│ Precip:5%   │  Precip: 80%               │ ← precip chance FreeSans9pt, TC_DATUM
     │          │              │                            │
     │          │    ← swipe card zone →                   │
 820 │ ◀        │              │                         ▶  │ ← scroll arrows: filled triangles
 840 │ (prev 3) │              │   more days ▶      (next 3)│   left shown when offset>0;
     │          │              │                            │   right triangle when offset+3<forecastDays;
     │          │              │                            │   hint «more days» only when offset==0 (MR_DATUM X=kWidth−42)
 940 │                    ○  ◉  ○                           │ ← pagination dot 2 (Forecast) active
 955 │                                     Updated: 14:35   │
 960 └──────────────────────────────────────────────────────┘
```

### Forecast Element Reference

| Y (px) | Element | Notes |
|-------:|---------|-------|
| 95 | `"Temperature"` title + Hi/Lo legend | FreeSansBold9pt, TC_DATUM; legend bars at kWidth−130/−75 |
| 120–180 | Temperature sparkline | chartH=60, padding=60; Hi = 5-px line, Lo = 1-px line |
| 180 | Sparkline x-axis + vertical axis at X=50 | MinT / MaxT labels at MR_DATUM X=45 |
| 270 | `"Rain Chance (%)"` title | FreeSansBold9pt, TC_DATUM |
| 290–340 | Precipitation bar chart | chartH=50, barW clamped 6–28 px |
| 405 | Full-width horizontal rule | |
| 408–625 | Three day-cards (180 px each), scrollable via `forecastOffset` | Cards 0–9 |
| 418 | Day label: `"Today"`, `"Mon 03"`, …  | FreeSans12pt, TC_DATUM |
| 476 | Weather icon, r=22 | Condition-matched vector |
| 522 | Condition text (truncated to 12 chars) | FreeSans9pt, TC_DATUM |
| 556 | `"H:xx  L:xx"` temps | FreeSans9pt, TC_DATUM |
| 581 | Temp range bar (100×7 px) | Proportional to 10-day min/max |
| 602 | `"Precip: xx%"` | FreeSans9pt, TC_DATUM |
| 820–860 | Left / right scroll triangles | Left: (10,840)→(30,820)→(30,860); Right: mirror |
| 840 | `"more days"` scroll hint | FreeSans9pt, MR_DATUM at X=kWidth−42; drawn only when `forecastOffset == 0` (avoids ambiguity with back-arrow on pages 2+) |
| 940 | Pagination (dot 2 filled) | |

---

## Page 3 — Settings & Diagnostics

Three tappable icon columns at the top; live diagnostic readouts below.  
A tap (release within 30 px threshold) selects the column under `tapX / 180`
and either executes immediately or sets `settingsCursor` for visual feedback.

| Column | X range | Icon | Action triggered |
|-------:|---------|------|-----------------|
| 0 | 0–179 | ↻ Sync | `_enterDeepSleepForImmediateWakeup()` — 1-second timer → normal fetch |
| 1 | 180–359 | ))) WiFi | Launch SoftAP + captive-portal provisioning (`ProvisioningManager`) |
| 2 | 360–540 | ☽ Sleep | `enterDeepSleep()` — 30-minute timer |

The `settingsCursor` (0/1/2) draws an inverted black rect over the selected column.

```
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │                                                      │
  15 │                                       65%  [███░░ ▌] │ ← battery
  80 │               Settings & Diagnostics                 │ ← FreeSansBold18pt, TC_DATUM
 130 ├──────────────────────────────────────────────────────┤
     │                                                      │
     │  ┌────────── col 0 (X 0–179) ─────────┐             │
 204 │  │ ┌──── selected highlight rect ────┐ │             │ ← rect top: Y=iconCY−r−18=204
     │  │ │                                 │ │             │   width: colW−16=164 px
 250 │  │ │       [ ↻  sync arc+head  ]     │ │  [))) wifi] │ ← icon cy=250, r=28; cx=colW/2+colW*i
     │  │ │                                 │ │  [☽  sleep] │   (cx = 90, 270, 450)
 332 │  │ │            Sync                 │ │  Setup Sleep│ ← label FreeSans18pt, MC_DATUM, Y=332
     │  │ │            (white on black)     │ │             │
 378 │  │ └─────────────────────────────────┘ │             │ ← rect bottom: Y=204+174=378
     │  └────────────────────────────────────┘             │   (iconR*2 + (labelY−iconCY) + 36)
 390 ├──────────────────────────────────────────────────────┤ ← rule
     │                                                      │
 420 │   Battery           │                 3.92 V  (75%) │ ← diagY=420, label TL / value TR, FreeSans12pt
 460 │   IP Address        │         192.168.1.105          │ ← diagY+40  (or "x.x.x.x (offline)")
 500 │   Firmware          │         v2.1.0 (main)          │ ← diagY+80  APP_VERSION + BUILD_TAG
 540 │   Last synced       │                   5 min ago    │ ← diagY+120 _lastSyncTime relative time
 580 │   Status            │               [00] OK          │ ← diagY+160 _lastError code + string
     │                                                      │
     │  Error code table (AppError enum):                   │
     │    00 = OK                                           │
     │    01 = WiFi connect failed                          │
     │    02 = NTP sync failed (using RTC)                  │
     │    03 = Weather API fetch failed                     │
     │    04 = Low battery — fetch skipped                  │
     │                                                      │
 940 │                    ○  ○  ◉                           │ ← pagination dot 3 (Settings) active
 955 │                                     Updated: 14:35   │
 960 └──────────────────────────────────────────────────────┘
```

### Settings Element Reference

| Y (px) | X (px) | Element | Notes |
|-------:|-------:|---------|-------|
| 80 | kWidth/2 | `"Settings & Diagnostics"` | FreeSansBold18pt, TC_DATUM |
| 130 | 20–520 | Horizontal rule | |
| 204–378 | colW×i+8 | Selection highlight rect (164×174 px) | Black fill; icon and label drawn white |
| 250 | 90 / 270 / 450 | Sync / WiFi / Sleep icon (r=28) | Vector icons; colour inverts when selected |
| 332 | 90 / 270 / 450 | Column label | FreeSans18pt, MC_DATUM |
| 320 | 40 / kWidth−40 | `"Battery"` label + `"x.xx V  (xx%)"` value | TL_DATUM / TR_DATUM, FreeSans12pt |
| 460 | 40 / kWidth−40 | `"IP Address"` label + IP value | TL / TR; `"(offline)"` suffix when stale |
| 500 | 40 / kWidth−40 | `"Firmware"` label + `"vX.Y.Z (tag)"` value | `APP_VERSION` build flag + `BUILD_TAG` |
| 540 | 40 / kWidth−40 | `"Last synced"` label + relative time | `"just now"`, `"N min ago"`, `"N hr ago"` |
| 580 | 40 / kWidth−40 | `"Status"` label + `"[HH] description"` | `_lastError` (persisted in `rtcLastError`) |
| 940 | 246/270/294/318us: [HH] description"` | `_lastError` (persisted in `rtcLastError`) |
| 940 | 246/270/294 | Pagination (dot 3 filled) | |

---

## Details Overlay

Triggered by a long-press (hold) on any page. Drawn by `renderActivePage()` after the normal page
content when `showOverlay == true`. Overlays the bottom 250 px of the canvas with a white background
and a double-thick top border, showing supplementary data not visible elsewhere on the current page.

```
Y ≈  ┌──────────────────────────────────────────────────────┐
     │ ... (page content above) ...                         │
 710 ══════════════════════════════════════════════════════════ ← double-thick 2-px rule (kHeight−250/249)
     │                                                      │
 734 │                   More Details                        │ ← FreeSansBold18pt7b, TC_DATUM
     │                                                      │
 775 │              AQI: 48  (Good)                          │ ← AQI value + EPA category, FreeSans12pt
     │                                                      │
 820 │        Tornado Watch  [EXTREME]                       │ ← alert headline + severity if data.hasAlert;
     │           — or —                                      │   truncated to ≤42 chars; FreeSans12pt
     │        No active weather alerts                       │   else reassurance string
     │                                                      │
 865 │              Dew Point: 14 C                          │ ← calculated: T − (100−RH)/5; FreeSans12pt
     │                                                      │
 960 └──────────────────────────────────────────────────────┘
```

**AQI categories** (EPA scale, computed inline from `data.aqi`):

| AQI range | Category |
|----------:|---------|
| 0–50 | Good |
| 51–100 | Moderate |
| 101–150 | Sensitive Groups |
| 151–200 | Unhealthy |
| 201–300 | Very Unhealthy |
| 301+ | Hazardous |

### Overlay Element Reference

| Y (px) | Element | Font / Notes |
|-------:|---------|------|
| kHeight−249/250 (711) | Double-thick horizontal rule | `drawFastHLine` × 2 |
| kHeight−226 (734) | `"More Details"` title | FreeSansBold18pt7b, TC_DATUM |
| kHeight−185 (775) | `"AQI: N  (Category)"` | FreeSans12pt7b, TC_DATUM; category from kAQIBreaks[] lookup |
| kHeight−140 (820) | Alert headline or `"No active weather alerts"` | FreeSans12pt7b, TC_DATUM; truncated to 42 chars + `…` |
| kHeight−95 (865) | `"Dew Point: N C"` | FreeSans12pt7b, TC_DATUM; `data.tempC − (100 − data.humidity) / 5.0f` |

The `"Updated: HH:MM"` timestamp (BR_DATUM, Y=955) is **suppressed** on the Settings page, where
the `"Last synced"` diagnostic row already provides the same information.

---

## Loading Screen

Shown on first boot or when there is no valid RTC cache.  
Writes directly to `M5.Display` (no sprite buffering). No pagination dots.  
Step advances via `updateLoadingStep(0/1/2/3)` using `epd_fastest` on the Y 450–720 zone.  
When cached data exists on a timer wakeup, `showRefreshingBadge()` writes a small strip at Y 910–960 instead.

```
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │                                                      │
  30 │               M5Paper Weather                        │ ← app title TextSize=2, centred
  68 │                   v2.0.0                             │ ← APP_VERSION TextSize=1, centred
 100 │               Fetching data for:                     │ ← context label TextSize=1, centred
 130 │                   Chicago, IL                        │ ← city name TextSize=2, centred
     │                                                      │
     │            ☀  ←  sun (upper-left, 1 px thin border) │
     │            ╔══════════════╗                          │
 300 │            ║  cloud+sun  ║                          │ ← cx=270, cy=300, scale s=55
     │            ║   outline   ║  (cloud: 3 px border;    │   Main cloud circle r=55
     │            ╚══════════════╝   sun: 1 px border)     │   Lobes r=38.5; flat base rect
     │                                                      │   Sun cx=221,cy=251,r=33 + 6 rays (1px)
 430 │ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  │ ← double divider Y=430/431
     │                                                      │
 470 │    ┌─[░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░]─┐  │ ← progress bar barX=70,barW=400,barH=18
     │    │  step 0 → 0% · step 1 → 33% · step 2 → 67%  │  │   rounded rect, bold border
     │    │  step 3 → 100% (all done)                    │  │
     │    └──────────────────────────────────────────────┘  │
     │                                                      │
     │    ──────────── ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    │ ← connector: solid (done) / dashed (pending)
 555 │         ◉                  ○                  ○      │ ← step dots at X=135/270/405, Y=555
     │        WiFi               Time             Weather   │   dot labels Y=577: TextSize=1
     │   (active=filled+ring) (pending=hollow×2) (done=✓)  │
     │                                                      │
 650 │             Connecting to WiFi.                      │ ← action label FreeSans12pt7b, centred
     │                                                      │
 780 │           Hold G38 to reconfigure                    │ ← hint TextSize=1, centred (static)
     │                                                      │
 920 │                                          v2.0.0      │ ← version TextSize=1, right-aligned (static)
 960 └──────────────────────────────────────────────────────┘
```

### Loading Step States

| `step` | Progress bar fill | Dot states (X=135 / 270 / 405) | Action label |
|-------:|------------------|-------------------------------|--------------|
| 0 | 0% (empty) | ◉ &nbsp; ○ &nbsp; ○ | `"Connecting to WiFi."` |
| 1 | 33% (131 px) | ✓ &nbsp; ◉ &nbsp; ○ | `"Syncing time.."` |
| 2 | 67% (263 px) | ✓ &nbsp; ✓ &nbsp; ◉ | `"Fetching weather..."` |
| 3 | 100% (394 px) | ✓ &nbsp; ✓ &nbsp; ✓ | `"Done!"` |

**Dot render key:**  
◉ = active → `fillCircle(r=13, black)` + `fillCircle(r=6, white)` inner ring  
✓ = complete → `fillCircle(r=13, black)` + white checkmark strokes  
○ = pending → `drawCircle(r=13)` + `drawCircle(r=12)` double-hollow ring

### Refreshing Badge (timer wakeup with cached data)

`showRefreshingBadge()` partial-refreshes Y 910–960 at `epd_fastest`:
```
 910 │          Updating weather...                         │ ← TextSize=1, centred, Y=926
 960 └──────────────────────────────────────────────────────┘
```

---

## Provisioning Screen

Shown when the device has no WiFi credentials, or when the user taps **Setup**.
Direct `M5.Display` calls only — no sprite, no pagination.

The e-ink screen shows a QR code for joining the SoftAP (`M5Paper-ABCD`) and the
portal URL. All actual WiFi configuration happens in the **web portal** served at
`http://192.168.4.1`.

```
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │                                                      │
  28 │           Scan to Connect & Configure                │ ← FreeSansBold18pt7b, centred
  70 │    Scan QR to join WiFi, then open the URL below    │ ← FreeSans9pt7b, centred
     │                                                      │
     │       ┌────────────────────────────────────┐         │
     │       │  ██████  █  ██   ██    ██  ██████  │         │
     │       │  ██  ██  ████     ████     ██  ██  │         │
 140 │       │  ██████  ██ ██ ██ ██████   ██████  │         │ ← QR code 37×37 modules × 6 px/module
     │       │   (222 × 222 px, centred at X=270)  │         │   qrOX=(540−222)/2=159, qrOY=140
 362 │       └────────────────────────────────────┘         │   encodes WIFI:T:nopass;S:<ssid>;;
     │                                                      │
 398 │                 Network: M5Paper-ABCD                │ ← SSID, FreeSansBold12pt7b (captionY=398)
 438 │                 http://192.168.4.1                   │ ← AP URL, FreeSans12pt7b (captionY+40)
 474 │                  No password required                │ ← FreeSans9pt7b (captionY+76)
     │                                                      │
 960 └──────────────────────────────────────────────────────┘
```

### Web Portal — WiFi Networks Section

The portal WiFi fieldset supports up to **5 networks**. The first entry is labelled
*primary* and is always present. Additional networks can be added with **+ Add
Network** (up to 5 total) or removed with **× Remove**.

```
 ╔═══════════════════════════════════════════════════╗
 ║  📶 WiFi Networks                                 ║
 ║  ┌─────────────────────────────────────────────┐  ║
 ║  │ NETWORK 1 (primary)                         │  ║
 ║  │  SSID     [Home-Network          ]          │  ║
 ║  │  Password [••••••                ]          │  ║
 ║  └─────────────────────────────────────────────┘  ║
 ║  ┌─────────────────────────────────────────────┐  ║
 ║  │ NETWORK 2                    [× Remove]     │  ║
 ║  │  SSID     [Office-5GHz        ]             │  ║
 ║  │  Password [••••••••           ]             │  ║
 ║  └─────────────────────────────────────────────┘  ║
 ║  [+ Add Network]                                  ║
 ║  Up to 5 networks. Device connects to             ║
 ║  whichever is available with strongest signal.    ║
 ╚═══════════════════════════════════════════════════╝
```

Form fields posted as `ssid_0`/`pass_0` … `ssid_4`/`pass_4` (gaps from removed
entries are skipped server-side). The device stores them in NVS as `w_ssid_0` …
`w_ssid_4`, `w_pass_0` … `w_pass_4`, and `w_count`.

---

## PIN Pad Screen

Shown before provisioning when an admin PIN is configured in NVS.

```
Y ≈  ┌──────────────────────────────────────────────────────┐
   0 │                                                      │
 100 │                  Enter Admin PIN                     │ ← message TextSize=2
     │                                                      │
 170 │          ┌────────────────────────────────┐          │ ← PIN entry box (height=60, X=60)
     │          │           * * * *              │          │ ← masked asterisks TextSize=3
 230 │          └────────────────────────────────┘          │
     │                                                      │
     │  startX=87, BW=110, BH=90, GAP=18                   │
     │         col0(X=87)   col1(X=215)   col2(X=343)      │
 320 │       ┌─────────┐  ┌─────────┐  ┌─────────┐         │ ← row 0
     │       │    1    │  │    2    │  │    3    │         │
     │       └─────────┘  └─────────┘  └─────────┘         │
 428 │       ┌─────────┐  ┌─────────┐  ┌─────────┐         │ ← row 1
     │       │    4    │  │    5    │  │    6    │         │
     │       └─────────┘  └─────────┘  └─────────┘         │
 536 │       ┌─────────┐  ┌─────────┐  ┌─────────┐         │ ← row 2
     │       │    7    │  │    8    │  │    9    │         │
     │       └─────────┘  └─────────┘  └─────────┘         │
 644 │       ┌─────────┐  ┌─────────┐  ┌─────────┐         │ ← row 3
     │       │    ←    │  │    0    │  │   OK    │         │   OK bg = 0x1E3A5F (dark blue)
     │       └─────────┘  └─────────┘  └─────────┘         │
 960 └──────────────────────────────────────────────────────┘
```

---

## Weather Icon Vocabulary

All icons are rendered on the off-screen canvas in pure black on white.

| Condition substring match | Icon | Rendered geometry |
|--------------------------|------|------------------|
| `"sun"` / `"clear"` | ☀ Sun | Filled circle (r=size) + 8 tri-stroke rays (size+6 → size+18) |
| `"partly"` | ⛅ Partly cloudy | Small hollow sun (upper-left) + cloud overlay |
| `"rain"` / `"shower"` | 🌧 Rain cloud | Cloud body (3 lobes + base rect) + 3 rounded raindrops |
| `"thunder"` / `"storm"` | ⛈ Thunderstorm | Cloud body + large lightning-bolt filled triangle |
| `"snow"` | 🌨 Snow cloud | Cloud body + 3 filled circular flakes below |
| default | ☁ Cloud | Three overlapping filled lobes + flat base rect |

`"partly"` check runs only in the default branch, so `"partly cloudy"` resolves to the default cloud path with the hollow sun peeking behind.

---

## AQI Gauge Detail

```
          _........_
        /    arc     \
       |  (180°→360°) |
       |               |
        \    / needle /
         \  / (fills)
          \/   ●  ← pivot (r=6)
        (cx=135, cy=570)
           AQI
```

- Half-arc track: `drawArc(r=45, r−8=37, 180°→360°)` (inner hollow = 8 px thick)
- Needle: filled triangle from pivot to arc tip; angle = `180 + (aqi / 300) * 180`
- Label `"AQI"` at (cx, cy+18), value at (cx, cy−18) — both MC_DATUM FreeSansBold9pt

---

## Sun Arc Detail

```
          _........_
        /   sky dome  \
       |  (180°→360°)  |
       |     ──☀──     | ← sun position = progress × 180° arc
       |                |   nighttime: crescent moon below horizon
       ─────────────────  ← horizon line (cx±60, 3 px tall)
        (cx=405, cy=570)
              Sun
```

- Dome: `drawArc(r=45, r−4=41, 180°→360°)` (4 px thick)
- Horizon: `fillRect(cx−60, cy−1, 120, 3)` (extends 15 px beyond arc radius)
- Daytime sun: hollow circle (r=7, double outline) + cross rays at position along arc
- Nighttime: filled crescent at (cx, cy+14)

---

## E-ink Refresh Mode Summary

| Trigger | API | EPD mode | Pixels refreshed |
|---------|-----|----------|-----------------|
| 30-min wakeup fetch (any page) | `renderActivePage(..., fastMode=false, ...)` | `epd_quality` | Full 540×960 |
| Interactive minute-tick on Dashboard | `updateClockOnly(t, ntpFailed)` | `epd_fastest` | Y 0–95 strip |
| Forecast card scroll | `renderActivePage(..., fastMode=true, ...)` | `epd_fastest` | Full canvas |
| Loading step advance | `updateLoadingStep(step)` | `epd_fastest` | Y 450–720 zone |
| Ghost-cleanup cycle (every 20 redraws) | `ghostingCleanup()` | `epd_quality` | Full screen W→B→W |
| Provisioning / PIN pad | Direct `M5.Display.*` calls | `epd_quality` / `epd_fast` | Full screen |

`rtcGhostCount` in `RTC_DATA_ATTR` survives deep sleep and resets after the cleanup cycle fires.

---

*Coordinates sourced from `lib/Display/DisplayManager.cpp`. Accurate for firmware **v2.1.0**.*
