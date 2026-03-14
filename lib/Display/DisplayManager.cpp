#include "DisplayManager.h"
#include "weather_bitmaps.h"
#include <WiFi.h>
#include <qrcode.h>
#include <esp_log.h>

static const char* TAG = "DisplayManager";

DisplayManager& DisplayManager::getInstance() {
    static DisplayManager instance;
    return instance;
}

void DisplayManager::begin() {
    M5.Display.setRotation(0);  // Portrait: 540 wide × 960 tall
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK); // Transparent background
    _canvas.setColorDepth(1); // 1-bit footprint (B/W) pushes to DMA 4x faster!
    _canvas.createSprite(kWidth, kHeight);
}

void DisplayManager::clear() {
    M5.Display.fillScreen(TFT_WHITE);
    _canvas.fillSprite(TFT_WHITE);
}

// ── QR Code ───────────────────────────────────────────────────────────────────
// Strategy: render all QR modules into a 1-bit off-screen LGFX_Sprite
// (pure RAM writes), then push the entire image to the display in one
// pushSprite() call.  This replaces ~1,369 individual SPI-transaction
// fillRect() calls with a single burst transfer — much faster on e-ink.
void DisplayManager::_drawQR(const String& url, int ox, int oy, int moduleSize) {
    QRCode qrcode;
    uint8_t buf[qrcode_getBufferSize(5)];
    qrcode_initText(&qrcode, buf, 5, ECC_LOW, url.c_str());

    // Sprite dimensions include the quiet zone (1 module on each side)
    const int quiet   = moduleSize;                           // 1-module border
    const int canvasSz = qrcode.size * moduleSize + quiet * 2;

    LGFX_Sprite sprite(&M5.Display);
    sprite.setColorDepth(1);   // 1-bit mono — minimises heap use (~1 KB for 37-module QR)
    if (!sprite.createSprite(canvasSz, canvasSz)) {
        // Fallback: direct draw if sprite allocation fails
        ESP_LOGW(TAG, "Sprite alloc failed — falling back to direct draw");
        M5.Display.startWrite();
        for (uint8_t sy = 0; sy < qrcode.size; sy++) {
            for (uint8_t sx = 0; sx < qrcode.size; sx++) {
                uint32_t color = qrcode_getModule(&qrcode, sx, sy)
                                 ? TFT_BLACK : TFT_WHITE;
                M5.Display.fillRect(ox + sx * moduleSize,
                                    oy + sy * moduleSize,
                                    moduleSize, moduleSize, color);
            }
        }
        M5.Display.endWrite();
        return;
    }

    // Fill quiet zone white, then draw modules
    sprite.fillScreen(TFT_WHITE);
    for (uint8_t sy = 0; sy < qrcode.size; sy++) {
        for (uint8_t sx = 0; sx < qrcode.size; sx++) {
            if (qrcode_getModule(&qrcode, sx, sy)) {
                sprite.fillRect(quiet + sx * moduleSize,
                                quiet + sy * moduleSize,
                                moduleSize, moduleSize,
                                TFT_BLACK);
            }
            // White modules are already white from fillScreen — skip them
        }
    }

    // Single SPI burst: push the entire sprite
    sprite.pushSprite(ox - quiet, oy - quiet);
    sprite.deleteSprite();
}

void DisplayManager::showProvisioningScreen(const String& ssid,
                                            const String& apUrl) {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    clear();

    constexpr int moduleSize = 6;           // 37 modules × 6 = 222 px
    constexpr int qrSizePx   = 37 * moduleSize;
    constexpr int qrOX = (kWidth  - qrSizePx) / 2;
    constexpr int qrOY = 140;

    // ── Title ───────────────────────────────────────────────────
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("Scan to Connect & Configure", kWidth / 2, 28);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.drawCentreString("Scan QR to join WiFi, then open the URL below",
                                kWidth / 2, 70);

    // ── QR Code — encodes WIFI: URI (ZXing meCard format) ──────
    // Scanning this with Android / iOS will prompt the user to join
    // the open access point automatically, no manual SSID entry needed.
    // Format: WIFI:T:nopass;S:<ssid>;;
    String wifiUri = "WIFI:T:nopass;S:" + ssid + ";;";
    _drawQR(wifiUri, qrOX, qrOY, moduleSize);

    // ── Caption ─────────────────────────────────────────────────
    int captionY = qrOY + qrSizePx + 36;
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("Network: " + ssid, kWidth / 2, captionY);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawCentreString(apUrl, kWidth / 2, captionY + 40);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.drawCentreString("No password required", kWidth / 2, captionY + 76);

    ESP_LOGI(TAG, "Provisioning screen shown (WiFi URI: %s)", wifiUri.c_str());
}

// ── PIN Pad ───────────────────────────────────────────────────────────────────
DisplayManager::Rect DisplayManager::_drawButton(const String& label,
                                                  int x, int y, int w, int h,
                                                  uint32_t bg, uint32_t fg) {
    M5.Display.fillRoundRect(x, y, w, h, 10, bg);
    M5.Display.drawRoundRect(x, y, w, h, 10, TFT_DARKGREY);
    M5.Display.setTextColor(fg, bg);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString(label, x + w / 2, y + h / 2);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    return {x, y, w, h};
}

String DisplayManager::promptPIN(const String& message) {
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    clear();

    constexpr int BW = 110, BH = 90, GAP = 18;
    constexpr int startX = (kWidth - (3 * BW + 2 * GAP)) / 2;
    constexpr int startY = 320;

    // Layout: rows 1-3 = 1-9, row 4 = <bsp> 0 OK
    struct Key { String label; int row, col; };
    Key keys[] = {
        {"1",0,0},{"2",0,1},{"3",0,2},
        {"4",1,0},{"5",1,1},{"6",1,2},
        {"7",2,0},{"8",2,1},{"9",2,2},
        {"<",3,0},{"0",3,1},{"OK",3,2}
    };

    // Draw title
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(message, kWidth / 2, 100, 1);

    // Draw PIN entry box (shows asterisks)
    String entered = "";
    constexpr int boxY = 170, boxH = 60;
    auto redrawEntry = [&]() {
        M5.Display.fillRect(60, boxY, kWidth - 120, boxH, TFT_WHITE);
        M5.Display.drawRoundRect(60, boxY, kWidth - 120, boxH, 8, TFT_BLACK);
        String masked = "";
        for (size_t i = 0; i < entered.length(); i++) masked += "*";
        M5.Display.setTextSize(3);
        M5.Display.drawCentreString(masked, kWidth / 2, boxY + 14, 1);
        M5.Display.setTextSize(2);
    };
    redrawEntry();

    // Draw keypad
    for (auto& k : keys) {
        uint32_t bg = (k.label == "OK") ? 0x1E3A5F : 0x2A2A2A;
        uint32_t fg = TFT_WHITE;
        int kx = startX + k.col * (BW + GAP);
        int ky = startY + k.row * (BH + GAP);
        _drawButton(k.label, kx, ky, BW, BH, bg, fg);
    }

    // Touch event loop
    while (true) {
        M5.update();
        if (M5.Touch.getCount() > 0) {
            auto t = M5.Touch.getDetail();
            if (t.wasReleased()) {
                int tx = t.x, ty = t.y;
                for (auto& k : keys) {
                    int kx = startX + k.col * (BW + GAP);
                    int ky = startY + k.row * (BH + GAP);
                    if (tx >= kx && tx < kx + BW && ty >= ky && ty < ky + BH) {
                        if (k.label == "<") {
                            if (!entered.isEmpty())
                                entered.remove(entered.length() - 1);
                        } else if (k.label == "OK") {
                            if (entered.length() >= 4) {
                                // highlight OK briefly
                                _drawButton("OK", kx, ky, BW, BH, 0x4ade80, TFT_BLACK);
                                delay(200);
                                return entered;
                            }
                        } else if (entered.length() < 8) {
                            entered += k.label;
                        }
                        redrawEntry();
                        break;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


// ── Ghosting cleanup cycle ────────────────────────────────────────────────────
void DisplayManager::ghostingCleanup() {
    ESP_LOGI(TAG, "Running e-ink ghost cleanup cycle (W→B→W)");
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.fillScreen(TFT_WHITE);
    delay(500);
    M5.Display.fillScreen(TFT_BLACK);
    delay(500);
    M5.Display.fillScreen(TFT_WHITE);
    delay(300);
}

// ── Multi-Page Render Router ──────────────────────────────────────────────────
void DisplayManager::renderActivePage(const WeatherData& data,
                                      const struct tm& t,
                                      const String& city,
                                      bool fastMode,
                                      int forecastOffset,
                                      int settingsCursor,
                                      bool showOverlay) {
    _loadingScreenActive = false; // loading complete — dismiss splash
    M5.Display.setEpdMode(fastMode ? epd_mode_t::epd_fastest
                                   : epd_mode_t::epd_quality);
    if (!fastMode) clear();
    _canvas.fillSprite(TFT_WHITE);
    _drawBattery();

    // Draw pagination across all pages
    _drawPagination(4, static_cast<int>(_activePage));

    switch (_activePage) {
        case Page::Dashboard:
            drawPageDashboard(data, t, city);
            break;
        case Page::Forecast:
            drawPageForecast(data, forecastOffset);
            break;
        case Page::Hourly:
            showHourlyPage(data);
            break;
        case Page::Settings:
            drawPageSettings(settingsCursor);
            break;
    }

    if (showOverlay) {
        // Draw lower third overlay background
        _canvas.fillRect(0, kHeight - 250, kWidth, 250, TFT_WHITE);
        _canvas.drawFastHLine(0, kHeight - 250, kWidth, TFT_BLACK);
        _canvas.drawFastHLine(0, kHeight - 249, kWidth, TFT_BLACK); // Thick line

        _canvas.setFont(&fonts::FreeSansBold18pt7b);
        _canvas.setTextDatum(TC_DATUM);
        _canvas.drawString("More Details", kWidth / 2, kHeight - 226);

        _canvas.setFont(&fonts::FreeSans12pt7b);
        char overlayBuf[80];

        // AQI value + EPA category label (gauge shows needle; text gives precise category)
        static const char* kAQILabel[] = {
            "Good", "Moderate", "Sensitive Groups", "Unhealthy", "Very Unhealthy", "Hazardous"
        };
        static const int kAQIBreaks[] = { 50, 100, 150, 200, 300, INT_MAX };
        int aqiCat = 0;
        for (int i = 0; i < 6; i++) { if (data.aqi <= kAQIBreaks[i]) { aqiCat = i; break; } }
        snprintf(overlayBuf, sizeof(overlayBuf), "AQI: %d  (%s)", data.aqi, kAQILabel[aqiCat]);
        _canvas.drawString(overlayBuf, kWidth / 2, kHeight - 185);

        // Alert status — full headline + severity if active, otherwise reassurance
        if (data.hasAlert && data.alertHeadline[0] != '\0') {
            snprintf(overlayBuf, sizeof(overlayBuf), "%s [%s]", data.alertHeadline, data.alertSeverity);
            // Truncate to fit 540px at FreeSans12pt
            String s(overlayBuf);
            if (s.length() > 42) s = s.substring(0, 39) + "...";
            _canvas.drawString(s, kWidth / 2, kHeight - 140);
        } else {
            _canvas.drawString("No active weather alerts", kWidth / 2, kHeight - 140);
        }

        // Dew point — calculated from temp + humidity (not shown anywhere else)
        // Approximation: Td ≈ T − (100 − RH) / 5  (accurate ±1 °C for RH > 50 %)
        float dewC = data.tempC - (100 - data.humidity) / 5.0f;
        snprintf(overlayBuf, sizeof(overlayBuf), "Dew Point: %.0f C", dewC);
        _canvas.drawString(overlayBuf, kWidth / 2, kHeight - 95);
    }

    // Last-updated timestamp — skip on Settings where diagRow already shows it
    if (_activePage != Page::Settings) {
        _drawLastUpdated(data.fetchTime);
    }

    _canvas.pushSprite(0, 0);
}

void DisplayManager::drawPageDashboard(const WeatherData& data,
                                       const struct tm& t,
                                       const String& city) {

    _drawBattery();

    // Date / time header
    char timeBuf[32], dateBuf[48];
    strftime(timeBuf, sizeof(timeBuf), "%l:%M %p", &t);
    strftime(dateBuf,  sizeof(dateBuf),  "%A, %B %d %Y", &t);

    _canvas.setTextColor(TFT_BLACK); // Ensure canvas texts have transparent backgrounds
    
    _canvas.setFont(&fonts::FreeSansBold24pt7b);
    _canvas.setTextSize(2);
    // Draw "2:51 PM" at Y=20
    _canvas.drawCentreString(timeBuf, kWidth / 2, 20);

    // NTP failure badge — shown when sync timed out and BM8563 RTC is driving the clock
    if (_ntpFailed) {
        _canvas.setFont(nullptr);
        _canvas.setTextSize(1);
        _canvas.drawString("NTP!", kWidth - 44, 22);
        _canvas.setFont(&fonts::FreeSansBold24pt7b);
        _canvas.setTextSize(2);
    }

    // ── Main Content ──────────────────────────────────────────────────────────
    String dispCity = city;
    if (dispCity.isEmpty()) dispCity = "Unknown";

    // City
    _canvas.setFont(&fonts::FreeSans24pt7b);
    _canvas.setTextSize(1);
    // Draw "Peoria, IL" at Y=100
    _canvas.drawCentreString(dispCity, kWidth / 2, 110);
    
    // Date
    _canvas.setFont(&fonts::FreeSans18pt7b);
    // Draw "Sunday, March 08 2026" at Y=160
    _canvas.drawCentreString(dateBuf, kWidth / 2, 160);
    
    // Divider
    _canvas.drawFastHLine(20, 200, kWidth - 40, TFT_BLACK);

    if (!data.valid) {
        _canvas.setFont(&fonts::FreeSans18pt7b);
        _canvas.drawCentreString("Fetching weather...", kWidth / 2, 480);
        return;
    }

    // ── Hero Section (Icon + Temp) ────────────────────────────────────────────
    _drawWeatherIcon(data.condition, 140, 265, 80); // Shifted down to Y=265 and left to 140
    
    _canvas.setFont(&fonts::FreeSansBold24pt7b);
    _canvas.setTextSize(1.5f); // Reduced font size from 2.0 to 1.5
    char tempBuf[32];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f C", data.tempC);
    _canvas.drawString(tempBuf, 240, 245); // Aligned baseline nicely with the resized icon

    // Condition String
    _canvas.setFont(&fonts::FreeSans24pt7b);
    _canvas.setTextSize(1);
    _canvas.drawCentreString(data.condition, kWidth / 2, 330);

    // ── Details Grid ──────────────────────────────────────────────────────────
    _canvas.setFont(&fonts::FreeSans12pt7b);
    char buf1[32], buf2[32];

    snprintf(buf1, sizeof(buf1), "Feels: %.1f C", data.feelsLikeC);
    _canvas.drawString(buf1, 40, 390);

    // #3: Compass direction prefix on wind
    { static const char* kCompass[] = {"N","NE","E","SE","S","SW","W","NW"};
      const char* windDir = kCompass[((data.windDirDeg + 22) % 360) / 45];
      snprintf(buf2, sizeof(buf2), "%s %.0f km/h", windDir, data.windKph); }
    // Draw wind rose inline before speed value
    _drawWindRose(kWidth/2 + 20, 398, 15, data.windDirDeg);
    _canvas.drawString(buf2, kWidth/2 + 42, 390);

    // #2: Full label names
    snprintf(buf1, sizeof(buf1), "Humidity: %d%%", data.humidity);
    _canvas.drawString(buf1, 40, 430);
    snprintf(buf2, sizeof(buf2), "Clouds: %d%%", data.cloudCover);
    _canvas.drawString(buf2, kWidth/2 + 20, 430);

    snprintf(buf1, sizeof(buf1), "UV Index: %d", data.uvIndex);
    _canvas.drawString(buf1, 40, 470);
    snprintf(buf2, sizeof(buf2), "Visibility: %.0f km", data.visibilityKm);
    _canvas.drawString(buf2, kWidth/2 + 20, 470);

    // ── Environmental Dials ───────────────────────────────────────────────────
    _drawAQIGauge(data.aqi, 570);
    _drawSunArc(time(nullptr), data.sunriseTime, data.sunsetTime, 570);

    // Sunrise / sunset times flanking the sun arc label
    if (data.sunriseTime > 0 && data.sunsetTime > 0) {
        struct tm srTm, ssTm;
        localtime_r(&data.sunriseTime, &srTm);
        localtime_r(&data.sunsetTime, &ssTm);
        char srBuf[12], ssBuf[12];
        strftime(srBuf, sizeof(srBuf), "%I:%M", &srTm);
        strftime(ssBuf, sizeof(ssBuf), "%I:%M", &ssTm);
        int sunCX = kWidth / 2;
        _canvas.setFont(&fonts::FreeSansBold9pt7b);
        _canvas.setTextSize(1);
        _canvas.setTextDatum(MR_DATUM);
        _canvas.drawString(srBuf, sunCX - 48, 608);
        _canvas.setTextDatum(ML_DATUM);
        _canvas.drawString(ssBuf, sunCX + 48, 608);
        _canvas.setTextDatum(TL_DATUM);
    }

    // Moon phase dial — right third column
    {
        int moonCX = (kWidth * 5) / 6;
        int moonR  = 38;
        int moonCY = 555; // shifted up so label fits below without overlap
        _drawMoonPhase(time(nullptr), moonCX, moonCY, moonR);
        _canvas.setFont(&fonts::FreeSansBold9pt7b);
        _canvas.setTextSize(1);
        _canvas.setTextDatum(MC_DATUM);
        _canvas.drawString("Moon", moonCX, moonCY + moonR + 12);
        _canvas.setTextDatum(TL_DATUM);
    }

    // Divider
    _canvas.drawFastHLine(20, 635, kWidth - 40, TFT_BLACK);

    // ── Tomorrow Preview ──────────────────────────────────────────────────────
    if (data.forecastDays > 1) {
        const auto& tmr = data.forecast[1];
        _canvas.setTextDatum(TC_DATUM);

        _canvas.setFont(&fonts::FreeSans18pt7b);
        _canvas.drawString("Tomorrow", kWidth / 2, 655);

        _drawWeatherIcon(tmr.condition, kWidth / 2, 730, 56);

        // #4: Move text below icon bottom (730+56=786) to avoid overlap
        _canvas.setFont(&fonts::FreeSans18pt7b);
        _canvas.drawString(tmr.condition, kWidth / 2, 800);

        _canvas.setFont(&fonts::FreeSans12pt7b);
        char tBuf[48];
        snprintf(tBuf, sizeof(tBuf), "H: %.0f C   L: %.0f C   Precip: %d%%",
                 tmr.maxTempC, tmr.minTempC, tmr.precipChance);
        _canvas.drawString(tBuf, kWidth / 2, 835);

        _canvas.setTextDatum(TL_DATUM);
    }

    // ── Weather alert banner (bottom strip, shown when hasAlert is true) ───────
    if (data.hasAlert) {
        _drawAlertBanner(data.alertHeadline);
    }
}

void DisplayManager::drawPageForecast(const WeatherData& data, int forecastOffset) {
    // ── Temperature band sparkline ────────────────────────────────────────────
    _drawForecastSparkline(data, 120);

    // ── Precipitation bar chart ───────────────────────────────────────────────
    _drawPrecipBars(data, 290);

    _canvas.drawFastHLine(20, 405, kWidth - 40, TFT_BLACK);

    if (data.forecastDays > 0) {
        int maxItems  = std::min(3, data.forecastDays - forecastOffset);
        int itemWidth = kWidth / 3;

        // Precompute 10-day extremes for the per-card temp range bar
        float globalMin = data.forecast[0].minTempC;
        float globalMax = data.forecast[0].maxTempC;
        for (int i = 1; i < data.forecastDays; i++) {
            if (data.forecast[i].minTempC < globalMin) globalMin = data.forecast[i].minTempC;
            if (data.forecast[i].maxTempC > globalMax) globalMax = data.forecast[i].maxTempC;
        }
        float globalRange = (globalMax - globalMin < 1.0f) ? 1.0f : globalMax - globalMin;

        for (int i = 0; i < maxItems; i++) {
            int idx = forecastOffset + i;
            if (idx >= data.forecastDays || idx >= 10) break;

            const auto& f  = data.forecast[idx];
            int cx         = (i * itemWidth) + (itemWidth / 2);

            // Vertical column divider
            if (i > 0) {
                _canvas.drawFastVLine(i * itemWidth, 408, 500, TFT_BLACK);
            }

            _canvas.setTextDatum(TC_DATUM);

            // ── Day label from actual timestamp ───────────────────────────────
            _canvas.setFont(&fonts::FreeSans12pt7b);
            _canvas.setTextSize(1);
            if (idx == 0) {
                _canvas.drawString("Today", cx, 418);
            } else if (f.dayTime > 0) {
                struct tm dayTm;
                time_t dt = f.dayTime;
                localtime_r(&dt, &dayTm);
                char dayBuf[12];
                strftime(dayBuf, sizeof(dayBuf), "%a %d", &dayTm);
                _canvas.drawString(dayBuf, cx, 418);
            } else {
                char dayBuf[8];
                snprintf(dayBuf, sizeof(dayBuf), "Day %d", idx + 1);
                _canvas.drawString(dayBuf, cx, 418);
            }

            // ── Weather icon ──────────────────────────────────────────────────
            _drawWeatherIcon(f.condition, cx, 476, 52);

            // ── Condition text ────────────────────────────────────────────────
            _canvas.setFont(&fonts::FreeSans9pt7b);
            String cond = f.condition;
            if (cond.length() > 12) cond = cond.substring(0, 10) + "..";
            _canvas.drawString(cond, cx, 522);

            // ── H / L text ────────────────────────────────────────────────────
            char tempBuf[24];
            snprintf(tempBuf, sizeof(tempBuf), "H:%.0f  L:%.0f", f.maxTempC, f.minTempC);
            _canvas.drawString(tempBuf, cx, 556);

            // ── Temp range bar relative to 10-day span ────────────────────────
            {
                constexpr int barW = 100;
                constexpr int barH = 7;
                int barX = cx - barW / 2;
                int barY = 581;
                _canvas.drawRect(barX, barY, barW, barH, TFT_BLACK);
                int fillStart = (int)((f.minTempC - globalMin) / globalRange * barW);
                int fillEnd   = (int)((f.maxTempC - globalMin) / globalRange * barW);
                int fillW     = std::max(4, fillEnd - fillStart);
                _canvas.fillRect(barX + fillStart, barY, fillW, barH, TFT_BLACK);
            }

            // ── Precipitation chance (#9: Rain→Precip, covers snow too) ──
            char popBuf[16];
            snprintf(popBuf, sizeof(popBuf), "Precip: %d%%", f.precipChance);
            _canvas.drawString(popBuf, cx, 602);

            _canvas.setTextDatum(TL_DATUM);
        }

        // Scroll indicators
        if (forecastOffset > 0) {
            _canvas.fillTriangle(10, 840, 30, 820, 30, 860, TFT_BLACK);
        }
        if (forecastOffset + 3 < data.forecastDays) {
            _canvas.fillTriangle(kWidth-10, 840, kWidth-30, 820, kWidth-30, 860, TFT_BLACK);
            // #10: hint text only on first page (no back-arrow to create ambiguity)
            if (forecastOffset == 0) {
                _canvas.setFont(&fonts::FreeSans9pt7b);
                _canvas.setTextDatum(MR_DATUM);
                _canvas.drawString("more days", kWidth - 42, 840);
                _canvas.setTextDatum(TL_DATUM);
            }
        }
    }
}

void DisplayManager::showHourlyPage(const WeatherData& data) {
    _canvas.setFont(&fonts::FreeSansBold18pt7b);
    _canvas.setTextDatum(TC_DATUM);
    _canvas.drawString("Hourly Forecast", kWidth / 2, 60);
    _canvas.drawFastHLine(20, 110, kWidth - 40, TFT_BLACK);

    if (data.hourlyCount == 0 || !data.valid) {
        _canvas.setFont(&fonts::FreeSans12pt7b);
        _canvas.drawString("No hourly data available", kWidth / 2, 200);
        return;
    }

    const int cols = 4;
    const int rows = 6;
    const int colW = kWidth / cols; // 540 / 4 = 135
    const int startY = 140;
    const int rowH = 125;           // Space per hour block

    int count = (data.hourlyCount > 24) ? 24 : data.hourlyCount;

    for (int i = 0; i < count; i++) {
        const auto& h = data.hourly[i];
        int r = i / cols;
        int c = i % cols;
        int cx = c * colW + (colW / 2);
        int cy = startY + r * rowH;

        if (r >= rows) break; // only render up to the grid limit

        // #6: row divider above each new row (skip first row)
        if (r > 0 && c == 0) _canvas.drawFastHLine(0, cy - 2, kWidth, TFT_BLACK);
        // #6: column divider after each column (skip leftmost edge)
        if (c > 0) _canvas.drawFastVLine(c * colW, cy - 2, rowH, TFT_BLACK);

        // #8: 12-hour time format consistent with dashboard
        struct tm* timeinfo = localtime(&h.timestamp);
        char timeBuf[16];
        strftime(timeBuf, sizeof(timeBuf), "%l:%M%p", timeinfo);
        // Trim leading space that %l may produce
        const char* timeTrimmed = (timeBuf[0] == ' ') ? timeBuf + 1 : timeBuf;

        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.drawString(timeTrimmed, cx, cy);

        // WMO weather code → condition string (full Open-Meteo code table)
        const char* cond = "Clear";
        if (h.weatherCode == 0)                           cond = "Clear";
        else if (h.weatherCode <= 3)                      cond = "Partly Cloudy";
        else if (h.weatherCode == 45 || h.weatherCode == 48) cond = "Fog";
        else if (h.weatherCode == 51 || h.weatherCode == 53) cond = "Drizzle";
        else if (h.weatherCode == 55)                     cond = "Drizzle";
        else if (h.weatherCode == 56 || h.weatherCode == 57) cond = "Freezing Drizzle";
        else if (h.weatherCode == 61 || h.weatherCode == 63) cond = "Rain";
        else if (h.weatherCode == 65)                     cond = "Heavy Showers";
        else if (h.weatherCode == 66 || h.weatherCode == 67) cond = "Freezing Rain";
        else if (h.weatherCode == 71 || h.weatherCode == 73) cond = "Snow";
        else if (h.weatherCode == 75)                     cond = "Heavy Snow";
        else if (h.weatherCode == 77)                     cond = "Snow";
        else if (h.weatherCode == 80 || h.weatherCode == 81) cond = "Rain";
        else if (h.weatherCode == 82)                     cond = "Heavy Showers";
        else if (h.weatherCode == 85 || h.weatherCode == 86) cond = "Snow Showers";
        else if (h.weatherCode == 95)                     cond = "Thunderstorms";
        else if (h.weatherCode == 96 || h.weatherCode == 99) cond = "Thunderstorms";
        else if (h.weatherCode >= 1 && h.weatherCode <= 3)  cond = "Mostly Cloudy";

        _drawWeatherIcon(cond, cx, cy + 30, 36);

        // Temp
        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%.0f C", h.tempC);
        _canvas.drawString(tempBuf, cx, cy + 65);

        // Rain %
        if (h.precipChance > 0) {
            char popBuf[16];
            snprintf(popBuf, sizeof(popBuf), "%d%%", h.precipChance);
            _canvas.drawString(popBuf, cx, cy + 85);
        }

        // #7: wind speed on hourly card
        if (h.windKph > 0.5f) {
            char wBuf[10];
            snprintf(wBuf, sizeof(wBuf), "%.0fkm/h", h.windKph);
            _canvas.drawString(wBuf, cx, cy + 103);
        }
    }
}

void DisplayManager::drawPageSettings(int settingsCursor) {
    _canvas.setFont(&fonts::FreeSansBold18pt7b);
    _canvas.drawCentreString("Settings & Diagnostics", kWidth / 2, 80);
    _canvas.drawFastHLine(20, 130, kWidth - 40, TFT_BLACK);

    // ── 3-column icon grid ────────────────────────────────────────────────────
    const int numItems = 3;
    const int colW     = kWidth / numItems;  // 180 px per column
    const int iconCY   = 250;               // icon centre Y
    const int labelY   = 332;
    const int iconR    = 28;
    const char* labels[] = { "Sync", "Setup", "Sleep" };

    for (int i = 0; i < numItems; i++) {
        int cx = colW * i + colW / 2;

        uint32_t iconColor = TFT_BLACK;
        if (i == settingsCursor) {
            // Highlight rect covering icon + label
            int rectTop = iconCY - iconR - 18;
            int rectH   = iconR * 2 + (labelY - iconCY) + 36;
            _canvas.fillRect(colW * i + 8, rectTop, colW - 16, rectH, TFT_BLACK);
            iconColor = TFT_WHITE;
        }

        if (i == 0)      _drawIconSync(cx,  iconCY, iconR, iconColor);
        else if (i == 1) _drawIconWifi(cx,  iconCY, iconR, iconColor);
        else             _drawIconSleep(cx, iconCY, iconR, iconColor);

        _canvas.setFont(&fonts::FreeSans18pt7b);
        _canvas.setTextColor(iconColor);
        _canvas.setTextDatum(MC_DATUM);
        _canvas.drawString(labels[i], cx, labelY);
        _canvas.setTextDatum(TL_DATUM);
        _canvas.setTextColor(TFT_BLACK);
    }

    _canvas.drawFastHLine(20, 390, kWidth - 40, TFT_BLACK);

    // ── Diagnostics (#11: two-column label/value layout) ─────────────────────
    _canvas.setFont(&fonts::FreeSans12pt7b);
    constexpr int kLabelX = 40;
    constexpr int kValueX = kWidth - 40;
    int diagY = 420;

    // Helper: draw label left-aligned, value right-aligned
    auto diagRow = [&](const char* label, const String& value, int y) {
        _canvas.setTextDatum(TL_DATUM);
        _canvas.drawString(label, kLabelX, y);
        _canvas.setTextDatum(TR_DATUM);
        _canvas.drawString(value, kValueX, y);
        _canvas.setTextDatum(TL_DATUM);
    };

    char vBuf[32];
    snprintf(vBuf, sizeof(vBuf), "%.2f V  (%d%%)", _cachedBatVoltage, _cachedBatLevel);
    diagRow("Battery", vBuf, diagY);

    String ip = WiFi.localIP().toString();
    bool liveOnline = (ip != "0.0.0.0");
    if (liveOnline) {
        // Update cache in-place if we happen to be online right now
        strncpy(_lastKnownIP, ip.c_str(), sizeof(_lastKnownIP) - 1);
        _lastKnownIP[sizeof(_lastKnownIP) - 1] = '\0';
    }
    bool haveIP = (_lastKnownIP[0] != '\0');
    String ipVal;
    if (liveOnline)       ipVal = ip;
    else if (haveIP)      ipVal = String(_lastKnownIP) + " (offline)";
    else                  ipVal = "No data yet";
    diagRow("IP Address", ipVal, diagY + 40);

    diagRow("Firmware", "v" + String(APP_VERSION) + " (" + String(BUILD_TAG) + ")", diagY + 80);

    // #12: Last-sync timestamp from RTC-surviving fetchTime
    if (_lastSyncTime > 0) {
        time_t now = time(nullptr);
        long diffMin = (long)(now - _lastSyncTime) / 60;
        char syncBuf[32];
        if (diffMin < 1)        snprintf(syncBuf, sizeof(syncBuf), "just now");
        else if (diffMin < 60)  snprintf(syncBuf, sizeof(syncBuf), "%ld min ago", diffMin);
        else                    snprintf(syncBuf, sizeof(syncBuf), "%ld hr ago", diffMin / 60);
        diagRow("Last synced", syncBuf, diagY + 120);
    } else {
        diagRow("Last synced", "No data yet", diagY + 120);
    }

    // ── Last error code row ───────────────────────────────────────────────────
    static const char* kErrorStrings[] = {
        "OK",
        "WiFi connect failed",
        "NTP sync failed (using RTC)",
        "Weather API fetch failed",
        "Low battery \xe2\x80\x94 fetch skipped"
    };
    const char* errStr = (_lastError < 5) ? kErrorStrings[_lastError] : "Unknown error";
    char errBuf[64];
    snprintf(errBuf, sizeof(errBuf), "[%02X] %s", _lastError, errStr);
    diagRow("Status", errBuf, diagY + 160);
}


void DisplayManager::setLastKnownIP(const char* ip) {
    if (ip && ip[0] != '\0') {
        strncpy(_lastKnownIP, ip, sizeof(_lastKnownIP) - 1);
        _lastKnownIP[sizeof(_lastKnownIP) - 1] = '\0';
    }
}

// ── Clock-only partial refresh ────────────────────────────────────────────────
// Redraws only the tight HH:MM AM/PM rectangle to avoid any full-page flash.
void DisplayManager::updateClockOnly(const struct tm& localTime, bool ntpFailed) {
    // The time string is drawn at Y=20 with FreeSansBold24pt7b at size 2.
    // Measured worst-case bounds: full width, 90 px tall (Y 0..90).
    constexpr int kClockY  = 0;
    constexpr int kClockH  = 95;

    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%l:%M %p", &localTime);

    M5.Display.setEpdMode(epd_mode_t::epd_fastest);

    // White-fill just the clock strip on the display then redraw the text
    M5.Display.fillRect(0, kClockY, kWidth, kClockH, TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setFont(&fonts::FreeSansBold24pt7b);
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(timeBuf, kWidth / 2, 20); // no trailing font-number — honours setFont()

    if (ntpFailed) {
        // Small warning badge to the right of the time string
        M5.Display.setFont(nullptr);
        M5.Display.setTextSize(1);
        M5.Display.drawString("NTP!", kWidth - 44, 22);
    }
}

// ── Alert banner ─────────────────────────────────────────────────────────────
void DisplayManager::_drawAlertBanner(const char* headline) {
    if (!headline || headline[0] == '\0') return;
    // Inverted strip: black background, white text, full width, 32 px tall
    constexpr int kBannerY = 855;
    constexpr int kBannerH = 32;
    _canvas.fillRect(0, kBannerY, kWidth, kBannerH, TFT_BLACK);
    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.setTextSize(1);
    _canvas.setTextColor(TFT_WHITE);
    _canvas.setTextDatum(MC_DATUM);
    // Truncate to fit within the banner
    String headline_str = String(headline);
    if (headline_str.length() > 55) headline_str = headline_str.substring(0, 52) + "...";
    _canvas.drawString(String("⚠ ") + headline_str, kWidth / 2, kBannerY + 10);
    _canvas.setTextDatum(TL_DATUM);
    _canvas.setTextColor(TFT_BLACK);
}

void DisplayManager::showMessage(const String& title, const String& body) {
    // #5/#13: proper FreeSans fonts — no trailing font-number arg so setFont() is honoured
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    clear();
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString(title, kWidth / 2, 200);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawCentreString(body, kWidth / 2, 248);
    M5.Display.setFont(nullptr);
}

// ── Loading screen ────────────────────────────────────────────────────────────
void DisplayManager::showLoadingScreen(const String& city) {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    clear();
    _loadingScreenActive = true;

    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // ── App title & version ─────────────────────────────────────────────────
    M5.Display.setFont(&fonts::FreeSans18pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("M5Paper Weather", kWidth / 2, 28);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.drawCentreString("v" APP_VERSION, kWidth / 2, 72);

    // ── Contextual city label ───────────────────────────────────────────────
    M5.Display.drawCentreString("Fetching data for:", kWidth / 2, 102);
    M5.Display.setFont(&fonts::FreeSans18pt7b);
    M5.Display.drawCentreString(city, kWidth / 2, 132);
    M5.Display.setFont(nullptr); // restore default font

    // ── Static bottom hints (not refreshed by _drawLoadingProgress) ───────────
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("Hold G38 to reconfigure", kWidth / 2, 778);
    M5.Display.drawRightString("v" APP_VERSION, kWidth - 20, 918);
    M5.Display.setFont(nullptr);

    // Draw initial progress state (step 0 = WiFi, bar at 0%)
    _drawLoadingProgress(0);

    ESP_LOGI(TAG, "Loading screen shown for city: %s", city.c_str());
}

// ── Animated loading illustration ───────────────────────────────────────────
void DisplayManager::_drawLoadingIcon(int step) {
    // Cloud geometry — 5-lobe cumulus (proper flat-base silhouette).
    // Cloud shifts 15 px right on step 2 so the sun becomes more visible.
    const int cCX = (step == 2) ? 285 : 270;
    const int cCY = 315;
    const int  s  = 52; // cloud scale

    // Sun geometry — positioned above the cloud on steps 1-2, centred on step 3.
    const int sCX = 270;
    const int sCY = (step == 3) ? 290 : 200;
    const int  sR = (step == 3) ?  68 :  46;

    // ── Sun drawn first so the cloud renders in front ─────────────────────────
    if (step >= 1) {
        // 4 px outline ring: solid fill then white hollow
        M5.Display.fillCircle(sCX, sCY, sR,     TFT_BLACK);
        M5.Display.fillCircle(sCX, sCY, sR - 4, TFT_WHITE);
        if (step == 3) {
            // Bright inner core — step 3 full-sun frame reads as "radiant"
            M5.Display.fillCircle(sCX, sCY, sR - 14, TFT_BLACK);
        }
    }

    // ── Rays (steps 2 & 3) ────────────────────────────────────────────────────
    if (step >= 2) {
        const int numRays = 8;
        const int r1 = sR + 8;                          // ray base (just outside disc)
        const int r2 = sR + (step == 3 ? 38 : 22);     // ray tip
        for (int i = 0; i < numRays; i++) {
            float angle = i * (2.0f * PI / numRays);
            // 3 parallel lines per ray → ~3 px wide, tapered toward tip
            for (int w = -1; w <= 1; w++) {
                float wpx = cosf(angle + PI / 2.0f) * (float)w * 1.5f; // base offset
                float wpy = sinf(angle + PI / 2.0f) * (float)w * 1.5f;
                float tpx = cosf(angle + PI / 2.0f) * (float)w * 0.4f; // tip offset (narrower)
                float tpy = sinf(angle + PI / 2.0f) * (float)w * 0.4f;
                M5.Display.drawLine(
                    sCX + (int)(r1 * cosf(angle) + wpx),
                    sCY + (int)(r1 * sinf(angle) + wpy),
                    sCX + (int)(r2 * cosf(angle) + tpx),
                    sCY + (int)(r2 * sinf(angle) + tpy),
                    TFT_BLACK
                );
            }
        }
    }

    // ── Cloud (steps 0–2) ─────────────────────────────────────────────────────
    if (step < 3) {
        const int kBorder = (step == 0) ? 0 : 3; // solid on step 0, outline on steps 1-2

        // ── Solid silhouette pass (5 lobes + flat-base rect) ─────────────────
        M5.Display.fillCircle(cCX,           cCY - s*2/10, s,          TFT_BLACK);
        M5.Display.fillCircle(cCX - s*8/10,  cCY + s*3/10, s*75/100,   TFT_BLACK);
        M5.Display.fillCircle(cCX + s*8/10,  cCY + s*3/10, s*75/100,   TFT_BLACK);
        M5.Display.fillCircle(cCX - s*14/10, cCY + s*6/10, s*55/100,   TFT_BLACK);
        M5.Display.fillCircle(cCX + s*14/10, cCY + s*6/10, s*55/100,   TFT_BLACK);
        const int baseTop = cCY + s*8/10;
        M5.Display.fillRect(cCX - s*19/10, baseTop, s*38/10, s*8/10, TFT_BLACK);

        if (kBorder > 0) {
            // ── Hollow interior pass — white insets erase seams at lobe intersections
            M5.Display.fillCircle(cCX,           cCY - s*2/10, s - kBorder,          TFT_WHITE);
            M5.Display.fillCircle(cCX - s*8/10,  cCY + s*3/10, s*75/100 - kBorder,   TFT_WHITE);
            M5.Display.fillCircle(cCX + s*8/10,  cCY + s*3/10, s*75/100 - kBorder,   TFT_WHITE);
            M5.Display.fillCircle(cCX - s*14/10, cCY + s*6/10, s*55/100 - kBorder,   TFT_WHITE);
            M5.Display.fillCircle(cCX + s*14/10, cCY + s*6/10, s*55/100 - kBorder,   TFT_WHITE);
            M5.Display.fillRect(cCX - s*19/10 + kBorder, baseTop,
                                s*38/10 - kBorder * 2, s*8/10 - kBorder, TFT_WHITE);
        }

        // ── Diagonal rain strokes below cloud base (step 0: storm frame) ─────
        if (step == 0) {
            const int dropBaseY = baseTop + s*8/10 + 10;
            for (int col = 0; col < 7; col++) {
                int rx = cCX - 90 + col * 30;
                // Two parallel lines per stroke → 2 px thick
                M5.Display.drawLine(rx,     dropBaseY, rx - 6, dropBaseY + 16, TFT_BLACK);
                M5.Display.drawLine(rx + 1, dropBaseY, rx - 5, dropBaseY + 16, TFT_BLACK);
            }
        }
    }
}

void DisplayManager::_drawLoadingProgress(int step) {
    // Zone: Y 155–730 — cleared on every call; includes icon + progress elements.
    constexpr int kZoneTop = 155;
    constexpr int kZoneBot = 730;
    M5.Display.fillRect(0, kZoneTop, kWidth, kZoneBot - kZoneTop, TFT_WHITE);
    // Per-step animated weather illustration
    _drawLoadingIcon(step);
    // Divider separating illustration from progress area
    M5.Display.drawFastHLine(60, 430, kWidth - 120, TFT_BLACK);
    M5.Display.drawFastHLine(60, 431, kWidth - 120, TFT_BLACK);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // ── Progress bar ──────────────────────────────────────────────────────────
    constexpr int barX = 70, barY = 470, barW = kWidth - 140, barH = 18;
    M5.Display.drawRoundRect(barX, barY, barW, barH, 5, TFT_BLACK);
    M5.Display.drawRoundRect(barX+1, barY+1, barW-2, barH-2, 4, TFT_BLACK); // bold border
    // step 0=0%, 1=33%, 2=67%, 3=100% — starts empty so progress feels genuine
    int fillW = (barW - 6) * step / 3;
    if (fillW > 0) {
        M5.Display.fillRoundRect(barX + 3, barY + 3, fillW, barH - 6, 3, TFT_BLACK);
    }

    // ── Step dots with connector lines ──────────────────────────────────────
    constexpr int dotY = 555;
    const int dotXs[3] = { kWidth / 4, kWidth / 2, kWidth * 3 / 4 }; // 135, 270, 405
    const char* dotLabels[] = { "WiFi", "Time", "Weather" };
    // Trailing dot count follows step index (1→2→3) for a subtle in-progress cue.
    // Step 3 = all complete.
    const char* actionLabels[] = {
        "Connecting to WiFi.",
        "Syncing time..",
        "Fetching weather...",
        "Done!"
    };

    for (int i = 0; i < 2; i++) {
        int x1 = dotXs[i] + 14, x2 = dotXs[i + 1] - 14;
        if (i < step) {
            // Completed segment — filled thick line
            M5.Display.fillRect(x1, dotY - 2, x2 - x1, 5, TFT_BLACK);
        } else {
            // Pending segment — thin dashed line
            for (int dx = x1; dx < x2; dx += 8) {
                M5.Display.drawFastHLine(dx, dotY, 4, TFT_BLACK);
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        int dx = dotXs[i];
        if (i < step) {
            // Completed: filled circle with check mark
            M5.Display.fillCircle(dx, dotY, 13, TFT_BLACK);
            // Checkmark: \ and / lines
            M5.Display.drawLine(dx - 6, dotY,     dx - 2, dotY + 5, TFT_WHITE);
            M5.Display.drawLine(dx - 6, dotY + 1, dx - 2, dotY + 6, TFT_WHITE);
            M5.Display.drawLine(dx - 2, dotY + 5, dx + 6, dotY - 4, TFT_WHITE);
            M5.Display.drawLine(dx - 2, dotY + 6, dx + 6, dotY - 3, TFT_WHITE);
        } else if (i == step) {
            // Active: bold filled circle
            M5.Display.fillCircle(dx, dotY, 13, TFT_BLACK);
            M5.Display.fillCircle(dx, dotY, 6,  TFT_WHITE); // inner ring
        } else {
            // Pending: hollow double-ring
            M5.Display.drawCircle(dx, dotY, 13, TFT_BLACK);
            M5.Display.drawCircle(dx, dotY, 12, TFT_BLACK);
        }

        M5.Display.setFont(&fonts::FreeSans9pt7b);
        M5.Display.setTextSize(1);
        M5.Display.drawCentreString(dotLabels[i], dx, dotY + 22);
        M5.Display.setFont(nullptr);
    }

    // ── Current action label ──────────────────────────────────────────────────
    M5.Display.setFont(&fonts::FreeSans18pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString(actionLabels[step], kWidth / 2, 645);
    M5.Display.setFont(nullptr); // restore default font
}

void DisplayManager::showRefreshingBadge() {
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.fillRect(0, 910, kWidth, 50, TFT_WHITE);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("Updating weather...", kWidth / 2, 922);
    M5.Display.setFont(nullptr);
}

void DisplayManager::updateLoadingStep(int step) {
    if (!_loadingScreenActive) return;
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    _drawLoadingProgress(step);
    ESP_LOGI(TAG, "Loading step %d", step);
}

void DisplayManager::_drawBattery() {
    uint32_t now = millis();
    if (now - _lastBatUpdateMs > 5000 || _lastBatUpdateMs == 0) {
        // M5Paper uses a 1/2 voltage divider on GPIO 35 for battery ADC.
        int32_t pin_mv = analogReadMilliVolts(35);
        int32_t mv = pin_mv * 2;
        _cachedBatVoltage = mv / 1000.0f;

        // Standard 1S LiPo logic: ~4100mV is 100%, ~3200mV is 0%
        if (mv >= 4100) {
            _cachedBatLevel = 100;
        } else if (mv <= 3200) {
            _cachedBatLevel = 0;
        } else {
            _cachedBatLevel = (int)(((mv - 3200) * 100) / (4100 - 3200));
        }
        _lastBatUpdateMs = now;
        ESP_LOGI(TAG, "Battery Updated: %.2fV (%d%%)", _cachedBatVoltage, _cachedBatLevel);
    }

    int x = kWidth - 55;
    int y = 15;

    // Erase bounding box for fast-refresh overwrites (pure white background)
    _canvas.fillRect(x - 45, y, 100, 20, TFT_WHITE);

    // Bounding outer cell
    _canvas.drawRoundRect(x, y, 40, 20, 3, TFT_BLACK);
    // Positive terminal nub
    _canvas.fillRect(x + 40, y + 5, 4, 10, TFT_BLACK);
    
    // Internal fill calculation
    int fillW = (36 * _cachedBatLevel) / 100;
    if (fillW > 0) {
        _canvas.fillRect(x + 2, y + 2, fillW, 16, TFT_BLACK);
    }
    
    // Readout percentage text aligned dynamically to the left of the battery
    _canvas.setTextDatum(MR_DATUM);
    _canvas.setTextSize(1);
    _canvas.setFont(nullptr);
    _canvas.drawString(String(_cachedBatLevel) + "%", x - 5, y + 10);
    // Reset alignment
    _canvas.setTextDatum(TL_DATUM);
}

void DisplayManager::_drawAQIGauge(int aqi, int yOff) {
    int cx = kWidth / 6;
    int cy = yOff;
    int r = 45;
    
    // Draw the main half-circle track (bolder, exact black)
    _canvas.drawArc(cx, cy, r, r - 8, 180, 360, TFT_BLACK);
    
    // Map AQI (assuming US EPA max logical bound is 300 for the dial)
    int mappedAqi = std::min(aqi, 300);
    int angle = 180 + (mappedAqi * 180 / 300);
    float rad = angle * PI / 180.0;
    
    // Calculate needle endpoint
    int nx = cx + (r - 12) * cos(rad);
    int ny = cy + (r - 12) * sin(rad);
    
    // Draw thick needle using a filled triangle from the pivot
    float radLeft = (angle - 90) * PI / 180.0;
    float radRight = (angle + 90) * PI / 180.0;
    int px1 = cx + 4 * cos(radLeft);
    int py1 = cy + 4 * sin(radLeft);
    int px2 = cx + 4 * cos(radRight);
    int py2 = cy + 4 * sin(radRight);
    
    _canvas.fillTriangle(nx, ny, px1, py1, px2, py2, TFT_BLACK);
    _canvas.fillCircle(cx, cy, 6, TFT_BLACK); // heavier pivot
    
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextSize(1);
    _canvas.setTextDatum(MC_DATUM);
    _canvas.drawString("AQI", cx, cy + 18);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", aqi);
    _canvas.drawString(buf, cx, cy - 18);
    _canvas.setTextDatum(TL_DATUM);
}

void DisplayManager::_drawSunArc(time_t current, time_t sunrise, time_t sunset, int yOff) {
    int cx = kWidth / 2;
    int cy = yOff;
    int r = 45;
    
    // Draw the sky dome and horizon line (bolder)
    _canvas.drawArc(cx, cy, r, r - 4, 180, 360, TFT_BLACK);
    _canvas.fillRect(cx - r - 15, cy - 1, (r + 15) * 2, 3, TFT_BLACK);
    
    if (sunrise > 0 && sunset > sunrise) {
        if (current >= sunrise && current <= sunset) {
            // Daytime: Calculate fractional progress of the sun across the sky
            float progress = (float)(current - sunrise) / (float)(sunset - sunrise);
            int angle = 180 + (int)(progress * 180.0f);
            float rad = angle * PI / 180.0;
            
            // Plot Cartesian coordinates for the sun
            int sx = cx + r * cos(rad);
            int sy = cy + r * sin(rad);
            
            // Draw a cute sun vector graphic with bolder rays
            _canvas.fillCircle(sx, sy, 7, TFT_WHITE);
            _canvas.drawCircle(sx, sy, 7, TFT_BLACK);
            _canvas.drawCircle(sx, sy, 6, TFT_BLACK); // double border for thickness
            _canvas.fillRect(sx - 10, sy - 1, 20, 3, TFT_BLACK); // Bold horizontal rays
            _canvas.fillRect(sx - 1, sy - 10, 3, 20, TFT_BLACK); // Bold vertical rays
        } else {
            // Nighttime: Draw a moon silhouette resting below the horizon
            _canvas.fillCircle(cx, cy + 14, 8, TFT_BLACK);
            _canvas.fillCircle(cx - 3, cy + 11, 7, TFT_WHITE);
        }
    }
    
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextDatum(MC_DATUM);
    _canvas.drawString("Sun", cx, cy + 18);
    _canvas.setTextDatum(TL_DATUM);
}

void DisplayManager::_drawMoonPhase(time_t current, int cx, int cy, int r) {
    if (current <= 0) return;
    
    // Moon phase calculation based on Conway's fractional phase
    // 1 lunar cycle ≈ 29.530588 days (2551443 seconds)
    // Known new moon: 1970-01-07 20:35:00 UTC (unix timestamp 592500)
    double lunarCycle = 2551443.0; // seconds
    double elapsed = (double)current - 592500.0;
    double phaseFrac = fmod(elapsed / lunarCycle, 1.0);
    if (phaseFrac < 0) phaseFrac += 1.0;
    
    // Convert fraction (0.0 to 1.0) into visual phase mapped from (-1.0 to 1.0)
    // 0.0 = New Moon (black)
    // 0.25 = First Quarter (left half black)
    // 0.5 = Full Moon (white)
    // 0.75 = Last Quarter (right half black)
    // 1.0 = New Moon
    
    // Base is a full moon (hollow outline so it's visible)
    _canvas.drawCircle(cx, cy, r, TFT_BLACK);
    _canvas.fillCircle(cx, cy, r - 1, TFT_WHITE);
    
    // Create the phase mask using scanlines
    for (int y = -r + 1; y <= r - 1; y++) {
        int xBound = (int)sqrt(r * r - y * y); // half-width of the circle at this Y
        
        if (phaseFrac < 0.5) {
            // Waxing: left side is shadowed
            // Center boundary moves from right (+xBound) to left (-xBound)
            double sweep = 1.0 - (phaseFrac * 4.0); // 0 -> 1.0; 0.25 -> 0.0; 0.5 -> -1.0
            int xShadow = (int)(xBound * sweep);
            
            // Draw shadow from left edge up to the sweeping boundary
            for (int x = -xBound; x <= xShadow; x++) {
                if (x < xBound) { // Avoid overdrawing the right boundary outline
                    _canvas.drawPixel(cx + x, cy + y, TFT_BLACK);
                }
            }
        } else {
            // Waning: right side is shadowed
            // Center boundary moves from left (-xBound) to right (+xBound)
            double sweep = ((phaseFrac - 0.5) * 4.0) - 1.0; // 0.5 -> -1.0; 0.75 -> 0.0; 1.0 -> 1.0
            int xShadow = (int)(xBound * sweep);
            
            // Draw shadow from the sweeping boundary up to the right edge
            for (int x = xShadow; x <= xBound; x++) {
                if (x > -xBound) { // Avoid overdrawing the left boundary outline
                    _canvas.drawPixel(cx + x, cy + y, TFT_BLACK);
                }
            }
        }
    }
}

void DisplayManager::_drawWindRose(int cx, int cy, int r, int direction) {
    // Outer dial ring
    _canvas.drawCircle(cx, cy, r, TFT_BLACK);
    
    // Draw 8 tick marks
    for (int i = 0; i < 8; i++) {
        float angle = i * 45.0 * PI / 180.0;
        int len = (i % 2 == 0) ? 4 : 2; // cardinal ticks longer than ordinal ticks
        int tx1 = cx + (r - len) * sin(angle); // 0 degrees = North = up (sin/cos swapped/negated below)
        int ty1 = cy - (r - len) * cos(angle);
        int tx2 = cx + r * sin(angle);
        int ty2 = cy - r * cos(angle);
        _canvas.drawLine(tx1, ty1, tx2, ty2, TFT_BLACK);
    }
    
    // Convert mathematical angle (0 is right, counter-clockwise) 
    // to compass angle (0 is up, clockwise)
    float rad = direction * PI / 180.0;
    
    // Calculate needle points
    int tipX = cx + (r - 2) * sin(rad);
    int tipY = cy - (r - 2) * cos(rad);
    
    int base1X = cx + (r / 3) * sin(rad + 0.8 * PI);
    int base1Y = cy - (r / 3) * cos(rad + 0.8 * PI);
    
    int base2X = cx + (r / 3) * sin(rad - 0.8 * PI);
    int base2Y = cy - (r / 3) * cos(rad - 0.8 * PI);
    
    // Draw solid filled needle indicating wind direction
    _canvas.fillTriangle(tipX, tipY, base1X, base1Y, base2X, base2Y, TFT_BLACK);
    
    // Small pivot dot
    _canvas.fillCircle(cx, cy, 2, TFT_WHITE);
    _canvas.drawCircle(cx, cy, 2, TFT_BLACK);
}

void DisplayManager::_drawWeatherIcon(const char* condition, int x, int y, int size) {
    String cond = condition;
    cond.toLowerCase();

    // Select the best matching icon from the IcoMoon weather icon set.
    // Conditions arrive as Google Weather API description text (e.g. "Heavy Rain").
    const uint8_t* bmp = icon_cloudy_bmp; // default fallback

    if (cond.indexOf("tornado") >= 0) {
        bmp = icon_tornado_bmp;
    } else if (cond.indexOf("hurricane") >= 0 || cond.indexOf("tropical storm") >= 0) {
        bmp = icon_hurricane_bmp;
    } else if (cond.indexOf("hail") >= 0) {
        bmp = icon_hail_bmp;
    } else if (cond.indexOf("thunder") >= 0 || cond.indexOf("t-storm") >= 0 || cond.indexOf("tstorm") >= 0) {
        bmp = icon_thunder_bmp;
    } else if (cond.indexOf("freezing") >= 0) {
        bmp = icon_freezing_rain_bmp;
    } else if (cond.indexOf("sleet") >= 0 || cond.indexOf("mixed") >= 0) {
        bmp = icon_sleet_bmp;
    } else if (cond.indexOf("blowing snow") >= 0 || cond.indexOf("blizzard") >= 0) {
        bmp = icon_blowing_snow_bmp;
    } else if (cond.indexOf("heavy snow") >= 0) {
        bmp = icon_heavy_snow_bmp;
    } else if (cond.indexOf("snow") >= 0 || cond.indexOf("flurr") >= 0) {
        bmp = icon_snow_bmp;
    } else if (cond.indexOf("heavy shower") >= 0 || cond.indexOf("heavy rain") >= 0) {
        bmp = icon_heavy_showers_bmp;
    } else if (cond.indexOf("drizzle") >= 0) {
        bmp = icon_drizzle_bmp;
    } else if (cond.indexOf("rain") >= 0 || cond.indexOf("shower") >= 0) {
        bmp = icon_rain_bmp;
    } else if (cond.indexOf("fog") >= 0) {
        bmp = icon_foggy_bmp;
    } else if (cond.indexOf("haze") >= 0 || cond.indexOf("mist") >= 0) {
        bmp = icon_haze_bmp;
    } else if (cond.indexOf("smoke") >= 0 || cond.indexOf("ash") >= 0) {
        bmp = icon_smoky_bmp;
    } else if (cond.indexOf("dust") >= 0 || cond.indexOf("sand") >= 0) {
        bmp = icon_haze_bmp;
    } else if (cond.indexOf("blustery") >= 0 || cond.indexOf("squall") >= 0) {
        bmp = icon_blustery_bmp;
    } else if (cond.indexOf("wind") >= 0 || cond.indexOf("breezy") >= 0) {
        bmp = icon_windy_bmp;
    } else if (cond.indexOf("mostly cloudy") >= 0 || cond.indexOf("overcast") >= 0) {
        bmp = icon_mostly_cloudy_bmp;
    } else if (cond.indexOf("partly cloudy") >= 0 || cond.indexOf("partly sunny") >= 0) {
        bmp = icon_partly_cloudy_bmp;
    } else if (cond.indexOf("cloudy") >= 0) {
        bmp = icon_cloudy_bmp;
    } else if (cond.indexOf("sun") >= 0 || cond.indexOf("clear") >= 0 || cond.indexOf("fair") >= 0) {
        bmp = icon_clear_bmp;
    } else if (cond.indexOf("n/a") >= 0 || cond.indexOf("unknown") >= 0 || cond.length() == 0) {
        bmp = icon_not_available_bmp;
    }

    // Nearest-neighbour scale the 32x32 XBM to size×size pixels, centred on (x, y).
    static constexpr int SRC_W = 32;
    static constexpr int SRC_H = 32;
    static constexpr int BYTES_PER_ROW = (SRC_W + 7) / 8; // 4
    int out_w = size;
    int out_h = size;
    int draw_x = x - out_w / 2;
    int draw_y = y - out_h / 2;
    for (int oy = 0; oy < out_h; oy++) {
        int sy = oy * SRC_H / out_h;
        for (int ox = 0; ox < out_w; ox++) {
            int sx = ox * SRC_W / out_w;
            uint8_t byte = pgm_read_byte(&bmp[sy * BYTES_PER_ROW + sx / 8]);
            bool dark = (byte >> (sx % 8)) & 1;
            _canvas.drawPixel(draw_x + ox, draw_y + oy, dark ? TFT_BLACK : TFT_WHITE);
        }
    }
}

// ── Settings page icons ───────────────────────────────────────────────────────

// Circular-arrow sync icon: near-full arc (gap at upper-right) + arrowhead.
void DisplayManager::_drawIconSync(int cx, int cy, int r, uint32_t color) {
    _canvas.drawArc(cx, cy, r, r - 5, 40, 320, color);

    // Arrowhead at end of arc (320°), pointing in direction of travel
    float endRad = 320.0f * PI / 180.0f;
    int   ex = cx + (int)(r * cosf(endRad));
    int   ey = cy + (int)(r * sinf(endRad));
    // Clockwise tangent: (-sin θ, cos θ)
    float tx = -sinf(endRad);   // ≈  0.643
    float ty =  cosf(endRad);   // ≈  0.766
    // Perpendicular (normal outward): (-ty, tx)
    float px = -ty, py = tx;
    int aw = r / 2, hw = r / 3;
    _canvas.fillTriangle(
        ex + (int)(tx * aw),              ey + (int)(ty * aw),        // tip
        ex + (int)(px * hw),              ey + (int)(py * hw),        // base L
        ex - (int)(px * hw),              ey - (int)(py * hw),        // base R
        color);
}

// WiFi-wave icon: three concentric upward arcs (∩) + centre dot.
void DisplayManager::_drawIconWifi(int cx, int cy, int r, uint32_t color) {
    // 220°→320° sweeps: upper-left → top (above centre) → upper-right = ∩ arch
    _canvas.drawArc(cx, cy, r,       r - 5,       220, 320, color);
    _canvas.drawArc(cx, cy, r * 2/3, r * 2/3 - 5, 220, 320, color);
    _canvas.drawArc(cx, cy, r / 3,   r / 3 - 4,   220, 320, color);
    _canvas.fillCircle(cx, cy, 5, color);
}

// Crescent-moon sleep icon: filled circle carved by offset background circle.
void DisplayManager::_drawIconSleep(int cx, int cy, int r, uint32_t color) {
    _canvas.fillCircle(cx, cy, r, color);
    // Carve crescent — use opposite colour so it works on both selected/unselected
    uint32_t bg = (color == TFT_WHITE) ? TFT_BLACK : TFT_WHITE;
    _canvas.fillCircle(cx + r / 3, cy - r / 5, (int)(r * 0.82f), bg);
}

void DisplayManager::_drawForecastSparkline(const WeatherData& data, int yOff) {
    if (data.forecastDays < 2) return;

    int padding = 60;
    int chartW = kWidth - (padding * 2);
    int chartH = 60;

    float minT = data.forecast[0].minTempC;
    float maxT = data.forecast[0].maxTempC;
    for (int i = 0; i < data.forecastDays; i++) {
        if (data.forecast[i].minTempC < minT) minT = data.forecast[i].minTempC;
        if (data.forecast[i].maxTempC > maxT) maxT = data.forecast[i].maxTempC;
    }
    float range = (maxT - minT < 1.0f) ? 1.0f : maxT - minT;

    // Axes
    _canvas.drawFastHLine(padding - 10, yOff + chartH, chartW + 20, TFT_BLACK);
    _canvas.drawFastVLine(padding - 10, yOff,           chartH,          TFT_BLACK);

    // Y-Axis labels
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextDatum(MR_DATUM);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f\xb0", maxT);
    _canvas.drawString(buf, padding - 15, yOff);
    snprintf(buf, sizeof(buf), "%.0f\xb0", minT);
    _canvas.drawString(buf, padding - 15, yOff + chartH);
    _canvas.setTextDatum(TL_DATUM);

    int stepX = chartW / (data.forecastDays - 1);
    int prevMaxX = -1, prevMaxY = -1;
    int prevMinX = -1, prevMinY = -1;

    for (int i = 0; i < data.forecastDays; i++) {
        int x     = padding + (i * stepX);
        int maxY  = yOff + chartH - (int)(((data.forecast[i].maxTempC - minT) / range) * chartH);
        int minY  = yOff + chartH - (int)(((data.forecast[i].minTempC - minT) / range) * chartH);

        // Max line — thick (5 px)
        _canvas.fillCircle(x, maxY, 4, TFT_BLACK);
        if (prevMaxX != -1) {
            _canvas.drawLine(prevMaxX, prevMaxY,   x, maxY,   TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY-1, x, maxY-1, TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY+1, x, maxY+1, TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY-2, x, maxY-2, TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY+2, x, maxY+2, TFT_BLACK);
        }

        // Min line — thin (1 px)
        _canvas.fillCircle(x, minY, 2, TFT_BLACK);
        if (prevMinX != -1) {
            _canvas.drawLine(prevMinX, prevMinY, x, minY, TFT_BLACK);
        }

        // X-axis tick
        _canvas.drawLine(x, yOff + chartH, x, yOff + chartH + 5, TFT_BLACK);

        prevMaxX = x; prevMaxY = maxY;
        prevMinX = x; prevMinY = minY;
    }

    // Title + inline legend
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextDatum(TC_DATUM);
    _canvas.drawString("Temperature", kWidth / 2, yOff - 25);
    // Legend: thick bar = High, thin bar = Low
    _canvas.fillRect(kWidth - 130, yOff - 20, 18, 4, TFT_BLACK);
    _canvas.drawString("Hi", kWidth - 108, yOff - 25);
    _canvas.fillRect(kWidth - 75, yOff - 18, 18, 2, TFT_BLACK);
    _canvas.drawString("Lo", kWidth - 53, yOff - 25);
    _canvas.setTextDatum(TL_DATUM);
}

void DisplayManager::_drawPrecipBars(const WeatherData& data, int yOff) {
    if (data.forecastDays < 1) return;

    int padding = 60;
    int chartW  = kWidth - (padding * 2);
    int chartH  = 50;
    int stepX   = (data.forecastDays > 1) ? chartW / (data.forecastDays - 1) : chartW;
    int barW    = std::max(6, std::min(28, stepX - 6));

    // Axes
    _canvas.drawFastHLine(padding - 10, yOff + chartH, chartW + 20, TFT_BLACK);
    _canvas.drawFastVLine(padding - 10, yOff,           chartH,          TFT_BLACK);

    // Y-axis label
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextDatum(MR_DATUM);
    _canvas.drawString("100", padding - 15, yOff);
    _canvas.drawString("0",   padding - 15, yOff + chartH);
    _canvas.setTextDatum(TL_DATUM);

    for (int i = 0; i < data.forecastDays; i++) {
        int cx   = padding + i * stepX;
        int barH = (int)(data.forecast[i].precipChance / 100.0f * chartH);
        if (barH > 1) {
            _canvas.fillRect(cx - barW / 2, yOff + chartH - barH, barW, barH, TFT_BLACK);
        }
        // X-axis tick
        _canvas.drawFastVLine(cx, yOff + chartH, 4, TFT_BLACK);
    }

    // Title
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextDatum(TC_DATUM);
    _canvas.drawString("Rain Chance (%)", kWidth / 2, yOff - 20);
    _canvas.setTextDatum(TL_DATUM);
}

void DisplayManager::_drawPagination(int totalPages, int currentPage) {
    if (totalPages <= 1) return;
    int dotSpacing = 24;
    int startX = (kWidth / 2) - ((totalPages - 1) * dotSpacing) / 2;
    constexpr int dotY = 940;
    // #14: page name label for the active page, drawn above the dots
    static const char* kPageNames[] = { "Dashboard", "Hourly", "Forecast", "Settings" };

    for (int i = 0; i < totalPages; i++) {
        int x = startX + (i * dotSpacing);
        if (i == currentPage) {
            _canvas.fillCircle(x, dotY, 6, TFT_BLACK); // Solid active dot
        } else {
            _canvas.drawCircle(x, dotY, 5, TFT_BLACK); // Hollow dot
            _canvas.drawCircle(x, dotY, 4, TFT_BLACK); // Thickened ring
        }
    }
    // Active page label centred above the dot strip
    if (currentPage >= 0 && currentPage < totalPages) {
        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.setTextDatum(BC_DATUM);
        _canvas.drawString(kPageNames[currentPage], kWidth / 2, dotY - 10);
        _canvas.setTextDatum(TL_DATUM);
    }
}

void DisplayManager::_drawLastUpdated(time_t fetchTime) {
    if (fetchTime <= 0) return;
    struct tm* ti = localtime(&fetchTime);
    char buf[64];
    // Formats into exactly: "Updated: 14:35"
    strftime(buf, sizeof(buf), "Updated: %H:%M", ti);
    
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextDatum(BR_DATUM);
    _canvas.drawString(buf, kWidth - 15, 955);
    _canvas.setTextDatum(TL_DATUM);
}
