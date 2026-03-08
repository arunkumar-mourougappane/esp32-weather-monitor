# Functionalities & Features

This document outlines the core user-facing functionalities implemented in the M5Paper Weather App v1.0.

## 🌤️ Weather Forecasting Engine

* **10-Day Deep Lookahead**: The device specifically overrides standard Google Weather API pagination caps (which default to 5 days) by enforcing `pageSize=10`. This allows the application to cleanly parse 10 contiguous days of highs, lows, precipitation chances, and prevailing condition states in a single JSON block.
* **Live Ambient Conditions**: Fetches real-time localized temperature, relative humidity metrics, dynamic "feels like" temp calculations, and wind variables.
* **Dynamic Screen Formatting**: Given that city names range from `"Rome"` to `"Llanfairpwllgwyngyll"`, the application engine natively concatenates the optional State parameters and utilizes hardware-accelerated bounding boxes (`drawCentreString`) to ensure the locale is perfectly horizontally aligned regardless of string length.

## 📱 Fluid E-Ink Experience

* **Instant Touch Response**: E-ink displays are notoriously slow. To counteract this, our input engine tracks Delta-X movements in real-time. This allows swipe thresholds (moving the forecast left or right) to execute mid-drag rather than forcing the user to lift their finger first, simulating an instantaneous UI feel.
* **Partial Refresh Pipeline**: Instead of flushing the screen buffer globally on every loop, the display driver is coded to only mutate localized boundary pixels. This allows the clock to tick every minute seamlessly and permits rapid scrolling through all 10 days of forecast without flashing the screen.

## ⚙️ Seamless Device Management

* **Zero-Code Setup Portal**: Users never have to hardcode Wi-Fi credentials or API tokens into `src` files. The device broadcasts its own AP and serves a modern, responsive mobile web app (`192.168.4.1`) where users can securely type in their settings.
* **Hardware Re-Provisioning**: Moving to a new house? Taking the clock to work? Holding down the side-rocker (G38) signals an immediate hardware interrupt that cleanly flags the NVS storage and forces a reboot back into configuration mode without risking memory corruption.
* **Atomic Time Correctness**: Deep sleep modes and unpowered states often corrupt internal Real-Time Clock memory hardware. The application proactively intercepts these invalid timelines and strictly halts runtime processes until `pool.ntp.org` returns verified, packet-completed UTC time.
