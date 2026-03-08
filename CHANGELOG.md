# Changelog

All notable changes to the `esp32-weather-monitor` project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.1] - 2026-03-08

### Added

* Comprehensive Doxygen API documentation across all 9 core `lib/` header files, improving overall project maintainability and developer onboarding.

### Changed

* Swapped non-standard `#pragma once` parser directives for robust C++ `#ifndef` macro include guards across all library headers to improve compilation portability on the Xtensa architecture.

---

## [1.0.0] - 2026-03-08

### Added

* Initial public release of the M5Paper ESP32 Weather Monitor.
* Integrated Google Weather API `v1` implementation parsing current conditions and 10-day forecasts (`pageSize=10`).
* Added POSIX-compliant NTP time synchronisation and timezone management mapping `localtime_r()` directly to ESP-IDF.
* 10-day forecast UI natively parsed onto the 4.7" e-ink device canvas.
* Provisioning Web Server using `ESPAsyncWebServer` running a captive portal setup page.
* Extensive Markdown documentation added in the `docs/` directory (`architecture.md` and `features.md`).

### Changed

* Complete architectural extract of monolithic source files from `src/` into 6 distinct PlatformIO `lib/` modules (`App`, `Config`, `Display`, `Input`, `Network`, `Provisioning`).
* Updated all core code to use universal library include paths (`#include <Header.h>`).
* Dramatically sped up the swiping mechanism in `InputManager` to eliminate the touch-release lag on continuous horizontal page scrolls.
* UI formatted to consistently centre the combined string `<City>, <State>`.
