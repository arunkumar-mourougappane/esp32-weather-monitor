# esp32-weather-monitor v3.1.0 Release Notes

See the full release notes in [`docs/releases/v3.1.0.md`](docs/releases/v3.1.0.md).

This is a patch/stability release focused on battery indicator improvements and four root-cause fixes for boot crashes, network failures, and silent input regressions introduced in v3.0.0.

## 🚀 Highlights

* **Reworked battery indicator** — hardware-accurate LiPo discharge curve, charging bolt icon, LOW badge, dashed fill at ≤ 15%, and a runtime-estimate row in Settings
* **Boot crash eliminated** — dedicated 24 KB FreeRTOS task for app logic avoids the `loopTask` stack-canary overflow during TLS handshake
* **NTP now succeeds on first attempt** — `SNTP_SYNC_MODE_IMMED` + race-condition fix; NTP syncs in < 2 seconds instead of exhausting all retries
* **AQI data now populated** — corrected invalid open-meteo parameter (`weed_pollen` → `ragweed_pollen`); AQI and pollen counts were zero in all prior releases
* **Touch / swipe fully restored** — `M5.update()` race between `loopTask` and `InputTask` was silently clearing all touch edge flags

## 🐛 Critical Fixes

* **`loopTask` stack-canary crash on every boot** — mbedTLS TLS handshake (~10 KB) overflows the 8 KB `loopTask` stack. Fixed by spawning `_appTask` via `xTaskCreatePinnedToCore()` with 24 KB of stack.
* **NTP always failing** — default `SNTP_SYNC_MODE_SMOOTH` never sets `SNTP_SYNC_STATUS_COMPLETED`; race condition re-queried volatile status after loop. Fixed with `SNTP_SYNC_MODE_IMMED` and a local `synced` bool.
* **AQI HTTP 400 / all pollen counts zero** — `weed_pollen` is not a valid open-meteo air-quality API variable (`ragweed_pollen` is correct). Every AQI request returned 400 Bad Request.
* **All touch events dropped in normal mode** — `loop()` and `InputTask` both called `M5.update()`; `loopTask` consistently cleared one-shot edge flags before `InputTask` could read them. Removed `M5.update()` from `loop()`.

## ⬆️ Upgrade Notes

1. No re-provisioning required — NVS layout is unchanged.
2. AQI and pollen data will be correct from the first fetch after flashing.
3. If `[02] NTP sync failed` was persistent in the Settings diagnostics under v3.0.0, this is resolved.
4. The `APP_VERSION` build flag is `3.1.0`.
