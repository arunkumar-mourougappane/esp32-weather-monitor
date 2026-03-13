#include "DisplayManager.h"
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
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString("Scan to Connect & Configure", kWidth / 2, 36, 1);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("Scan QR to join WiFi, then open the URL below",
                                kWidth / 2, 74, 1);

    // ── QR Code — encodes WIFI: URI (ZXing meCard format) ──────
    // Scanning this with Android / iOS will prompt the user to join
    // the open access point automatically, no manual SSID entry needed.
    // Format: WIFI:T:nopass;S:<ssid>;;
    String wifiUri = "WIFI:T:nopass;S:" + ssid + ";;";
    _drawQR(wifiUri, qrOX, qrOY, moduleSize);

    // ── Caption ─────────────────────────────────────────────────
    int captionY = qrOY + qrSizePx + 36;
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString("Network: " + ssid, kWidth / 2, captionY, 1);
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(apUrl, kWidth / 2, captionY + 44, 1);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("No password required", kWidth / 2, captionY + 82, 1);

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


// ── Multi-Page Render Router ──────────────────────────────────────────────────
void DisplayManager::renderActivePage(const WeatherData& data,
                                      const struct tm& t,
                                      const String& city,
                                      bool fastMode,
                                      int forecastOffset,
                                      int settingsCursor) {
    _loadingScreenActive = false; // loading complete — dismiss splash
    M5.Display.setEpdMode(fastMode ? epd_mode_t::epd_fastest
                                   : epd_mode_t::epd_quality);
    if (!fastMode) clear();
    _canvas.fillSprite(TFT_WHITE);
    _drawBattery();

    // Draw pagination across all pages
    _drawPagination(3, static_cast<int>(_activePage));

    switch (_activePage) {
        case Page::Dashboard:
            drawPageDashboard(data, t, city);
            break;
        case Page::Forecast:
            drawPageForecast(data, forecastOffset);
            break;
        case Page::Settings:
            drawPageSettings(settingsCursor);
            break;
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
    _drawWeatherIcon(data.condition, 140, 265, 40); // Shifted down to Y=265 and left to 140
    
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

    // 8-point compass from bearing degrees
    static const char* kWindDirs[] = {"N","NE","E","SE","S","SW","W","NW"};
    const char* windDir = kWindDirs[((data.windDirDeg + 22) / 45) % 8];

    snprintf(buf1, sizeof(buf1), "Feels: %.1f C", data.feelsLikeC);
    _canvas.drawString(buf1, 40, 390);
    snprintf(buf2, sizeof(buf2), "Wind: %.0f km/h %s", data.windKph, windDir);
    _canvas.drawString(buf2, kWidth/2 + 20, 390);

    snprintf(buf1, sizeof(buf1), "Hum: %d%%", data.humidity);
    _canvas.drawString(buf1, 40, 430);
    snprintf(buf2, sizeof(buf2), "Clouds: %d%%", data.cloudCover);
    _canvas.drawString(buf2, kWidth/2 + 20, 430);

    snprintf(buf1, sizeof(buf1), "UV: %d", data.uvIndex);
    _canvas.drawString(buf1, 40, 470);
    snprintf(buf2, sizeof(buf2), "Vis: %.0f km", data.visibilityKm);
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
        int sunCX = (kWidth * 3) / 4;
        _canvas.setFont(&fonts::FreeSansBold9pt7b);
        _canvas.setTextSize(1);
        _canvas.setTextDatum(MR_DATUM);
        _canvas.drawString(srBuf, sunCX - 48, 608);
        _canvas.setTextDatum(ML_DATUM);
        _canvas.drawString(ssBuf, sunCX + 48, 608);
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

        _drawWeatherIcon(tmr.condition, kWidth / 2, 730, 20);

        _canvas.setFont(&fonts::FreeSans18pt7b);
        _canvas.drawString(tmr.condition, kWidth / 2, 765);

        _canvas.setFont(&fonts::FreeSans12pt7b);
        char tBuf[48];
        snprintf(tBuf, sizeof(tBuf), "H: %.0f C   L: %.0f C   Rain: %d%%",
                 tmr.maxTempC, tmr.minTempC, tmr.precipChance);
        _canvas.drawString(tBuf, kWidth / 2, 810);

        _canvas.setTextDatum(TL_DATUM);
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
            _drawWeatherIcon(f.condition, cx, 476, 22);

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

            // ── Precipitation chance ──────────────────────────────────────────
            char popBuf[16];
            snprintf(popBuf, sizeof(popBuf), "Rain: %d%%", f.precipChance);
            _canvas.drawString(popBuf, cx, 602);

            _canvas.setTextDatum(TL_DATUM);
        }

        // Scroll indicators
        if (forecastOffset > 0) {
            _canvas.fillTriangle(10, 840, 30, 820, 30, 860, TFT_BLACK);
        }
        if (forecastOffset + 3 < data.forecastDays) {
            _canvas.fillTriangle(kWidth-10, 840, kWidth-30, 820, kWidth-30, 860, TFT_BLACK);
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

    // ── Diagnostics ───────────────────────────────────────────────────────────
    _canvas.setFont(&fonts::FreeSans12pt7b);
    int diagY = 440;

    char vBuf[64];
    snprintf(vBuf, sizeof(vBuf), "Battery: %.2f V  (%d%%)", _cachedBatVoltage, _cachedBatLevel);
    _canvas.drawString(vBuf, 40, diagY);

    String ip = WiFi.localIP().toString();
    bool liveOnline = (ip != "0.0.0.0");
    if (liveOnline) {
        // Update cache in-place if we happen to be online right now
        strncpy(_lastKnownIP, ip.c_str(), sizeof(_lastKnownIP) - 1);
        _lastKnownIP[sizeof(_lastKnownIP) - 1] = '\0';
    }
    bool haveIP = (_lastKnownIP[0] != '\0');
    String ipLine = "IP: ";
    if (liveOnline) {
        ipLine += ip;
    } else if (haveIP) {
        ipLine += String(_lastKnownIP) + " (offline)";
    } else {
        ipLine += "No data yet";
    }
    _canvas.drawString(ipLine, 40, diagY + 40);
    String verStr = "Firmware v" + String(APP_VERSION) + " (" + String(BUILD_TAG) + ")";
    _canvas.drawString(verStr, 40, diagY + 80);
}


void DisplayManager::setLastKnownIP(const char* ip) {
    if (ip && ip[0] != '\0') {
        strncpy(_lastKnownIP, ip, sizeof(_lastKnownIP) - 1);
        _lastKnownIP[sizeof(_lastKnownIP) - 1] = '\0';
    }
}

void DisplayManager::showMessage(const String& title, const String& body) {
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    clear();
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(title, kWidth / 2, 200, 1);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString(body, kWidth / 2, 260, 1);
}

// ── Loading screen ────────────────────────────────────────────────────────────
void DisplayManager::showLoadingScreen(const String& city) {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    clear();
    _loadingScreenActive = true;

    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // City name
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(city, kWidth / 2, 100, 1);

    // Cloud + sun icon centred around (kWidth/2, 300).
    // Technique: fill the entire merged silhouette BLACK first so overlapping
    // lobes fuse into one solid shape, then flood-fill the interior WHITE at
    // (radius - kBorder) to carve a clean uniform outline.  Drawing individual
    // circle outlines leaves interior arc seams visible where lobes intersect.
    int cx = kWidth / 2, cy = 300, s = 55;
    constexpr int kBorder = 3;

    // ── Step 1: solid merged silhouette ──────────────────────────────────────
    M5.Display.fillCircle(cx,          cy - s*2/10, s,      TFT_BLACK);
    M5.Display.fillCircle(cx - s*8/10, cy + s*4/10, s*7/10, TFT_BLACK);
    M5.Display.fillCircle(cx + s*8/10, cy + s*4/10, s*7/10, TFT_BLACK);
    int baseTop = cy + s*4/10;
    M5.Display.fillRect(cx - s*15/10, baseTop, s*30/10, s*7/10, TFT_BLACK);

    // ── Step 2: hollow interior — overlapping white insets eliminate seams ───
    // The merged white regions naturally erase every internal boundary arc.
    M5.Display.fillCircle(cx,          cy - s*2/10, s      - kBorder, TFT_WHITE);
    M5.Display.fillCircle(cx - s*8/10, cy + s*4/10, s*7/10 - kBorder, TFT_WHITE);
    M5.Display.fillCircle(cx + s*8/10, cy + s*4/10, s*7/10 - kBorder, TFT_WHITE);
    // Flat base interior: leave kBorder on sides and bottom; top is handled by
    // the circle insets above.
    M5.Display.fillRect(cx - s*15/10 + kBorder, baseTop,
                        s*30/10 - kBorder * 2, s*7/10 - kBorder, TFT_WHITE);

    // ── Sun peeking behind (upper-left), same fill→hollow technique ──────────
    int sunCX = cx - s*9/10, sunCY = cy - s*9/10, sunR = s*6/10;
    M5.Display.fillCircle(sunCX, sunCY, sunR,           TFT_BLACK);
    M5.Display.fillCircle(sunCX, sunCY, sunR - kBorder, TFT_WHITE);
    // 6 short rays
    for (int i = 0; i < 6; i++) {
        float rad = i * (PI / 3.0f);
        int x1 = sunCX + (int)((sunR + 4)  * cosf(rad));
        int y1 = sunCY + (int)((sunR + 4)  * sinf(rad));
        int x2 = sunCX + (int)((sunR + 14) * cosf(rad));
        int y2 = sunCY + (int)((sunR + 14) * sinf(rad));
        M5.Display.drawLine(x1, y1, x2, y2, TFT_BLACK);
        M5.Display.drawLine(x1+1, y1, x2+1, y2, TFT_BLACK);
    }

    // Divider below icon
    M5.Display.drawFastHLine(60, 430, kWidth - 120, TFT_BLACK);
    M5.Display.drawFastHLine(60, 431, kWidth - 120, TFT_BLACK);

    // Draw initial progress state (step 0 = WiFi)
    _drawLoadingProgress(0);

    ESP_LOGI(TAG, "Loading screen shown for city: %s", city.c_str());
}

void DisplayManager::_drawLoadingProgress(int step) {
    // Zone: Y 450 – 720  (cleared on each call for partial refresh)
    constexpr int kZoneTop = 450;
    constexpr int kZoneBot = 720;
    M5.Display.fillRect(0, kZoneTop, kWidth, kZoneBot - kZoneTop, TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // ── Progress bar ──────────────────────────────────────────────────────────
    constexpr int barX = 70, barY = 470, barW = kWidth - 140, barH = 18;
    M5.Display.drawRoundRect(barX, barY, barW, barH, 5, TFT_BLACK);
    M5.Display.drawRoundRect(barX+1, barY+1, barW-2, barH-2, 4, TFT_BLACK); // bold border
    int fillW = (barW - 6) * (step + 1) / 3;
    if (fillW > 0) {
        M5.Display.fillRoundRect(barX + 3, barY + 3, fillW, barH - 6, 3, TFT_BLACK);
    }

    // ── Step dots with connector lines ──────────────────────────────────────
    constexpr int dotY = 555;
    const int dotXs[3] = { kWidth / 4, kWidth / 2, kWidth * 3 / 4 }; // 135, 270, 405
    const char* dotLabels[] = { "WiFi", "Time", "Weather" };
    const char* actionLabels[] = {
        "Connecting to WiFi...",
        "Syncing time...",
        "Fetching weather..."
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

        M5.Display.setTextSize(1);
        M5.Display.drawCentreString(dotLabels[i], dx, dotY + 22, 1);
    }

    // ── Current action label ──────────────────────────────────────────────────
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(actionLabels[step], kWidth / 2, 650, 1);
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
    int cx = kWidth / 4; 
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
    int cx = (kWidth * 3) / 4;
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

void DisplayManager::_drawWeatherIcon(const char* condition, int x, int y, int size) {
    String cond = condition;
    cond.toLowerCase();
    
    if (cond.indexOf("sun") >= 0 || cond.indexOf("clear") >= 0) {
        // Sun Vector
        _canvas.fillCircle(x, y, size, TFT_BLACK);
        for(int i=0; i<8; i++) {
            float rad = i * (PI / 4.0);
            _canvas.drawLine(x + (size+6)*cos(rad), y + (size+6)*sin(rad),
                                x + (size+18)*cos(rad), y + (size+18)*sin(rad), TFT_BLACK);
            _canvas.drawLine(x + (size+6)*cos(rad)+1, y + (size+6)*sin(rad)+1,
                                x + (size+18)*cos(rad)+1, y + (size+18)*sin(rad)+1, TFT_BLACK);
            _canvas.drawLine(x + (size+6)*cos(rad)-1, y + (size+6)*sin(rad)-1,
                                x + (size+18)*cos(rad)-1, y + (size+18)*sin(rad)-1, TFT_BLACK);
        }
    } else if (cond.indexOf("rain") >= 0 || cond.indexOf("shower") >= 0) {
        // Rain Cloud Vector
        _canvas.fillCircle(x, y-size*0.2, size, TFT_BLACK);
        _canvas.fillCircle(x - size*0.8, y + size*0.4, size*0.7, TFT_BLACK);
        _canvas.fillCircle(x + size*0.8, y + size*0.4, size*0.7, TFT_BLACK);
        _canvas.fillRect(x - size*1.5, y + size*0.4, size*3.0, size*0.7, TFT_BLACK);
        // Cascading Rain Drops
        for(int i=-1; i<=1; i++) {
            _canvas.fillRoundRect(x + i*size*0.8 - 2, y + size*1.5 + (abs(i)*8), 5, size*0.8, 2, TFT_BLACK);
        }
    } else if (cond.indexOf("thunder") >= 0 || cond.indexOf("storm") >= 0) {
        // Lightning Storm Vector
        _canvas.fillCircle(x, y-size*0.2, size, TFT_BLACK);
        _canvas.fillCircle(x - size*0.8, y + size*0.4, size*0.7, TFT_BLACK);
        _canvas.fillCircle(x + size*0.8, y + size*0.4, size*0.7, TFT_BLACK);
        _canvas.fillRect(x - size*1.5, y + size*0.4, size*3.0, size*0.7, TFT_BLACK);
        // Large Lightning Bolt
        _canvas.fillTriangle(x+5, y + size*1.2, x - size*0.7, y + size*2.2, x + size*0.2, y + size*2.2, TFT_BLACK);
        _canvas.fillTriangle(x + size*0.2, y + size*2.2, x - size*0.4, y + size*3.2, x + size*0.5, y + size*2.0, TFT_BLACK);
    } else if (cond.indexOf("snow") >= 0) {
        // Snow Cloud Vector
        _canvas.fillCircle(x, y-size*0.2, size, TFT_BLACK);
        _canvas.fillCircle(x - size*0.8, y + size*0.4, size*0.7, TFT_BLACK);
        _canvas.fillCircle(x + size*0.8, y + size*0.4, size*0.7, TFT_BLACK);
        _canvas.fillRect(x - size*1.5, y + size*0.4, size*3.0, size*0.7, TFT_BLACK);
        // Hexagonal Flakes
        _canvas.fillCircle(x - size*0.6, y + size*1.6, 5, TFT_BLACK);
        _canvas.fillCircle(x, y + size*2.2, 5, TFT_BLACK);
        _canvas.fillCircle(x + size*0.6, y + size*1.6, 5, TFT_BLACK);
    } else {
        // Default Cloud (Partly Cloudy / Normal)
        if (cond.indexOf("partly") >= 0) {
            _canvas.fillCircle(x - size*0.6, y - size*0.6, size*0.8, TFT_BLACK); // Sun behind
            _canvas.fillCircle(x - size*0.6, y - size*0.6, size*0.8 - 6, TFT_WHITE);
        }
        _canvas.fillCircle(x, y, size, TFT_BLACK);
        _canvas.fillCircle(x - size*0.8, y + size*0.5, size*0.7, TFT_BLACK);
        _canvas.fillCircle(x + size*0.8, y + size*0.5, size*0.7, TFT_BLACK);
        _canvas.fillRect(x - size*1.5, y + size*0.5, size*3.0, size*0.7, TFT_BLACK);
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
    int y = 940;
    
    for (int i = 0; i < totalPages; i++) {
        int x = startX + (i * dotSpacing);
        if (i == currentPage) {
            _canvas.fillCircle(x, y, 6, TFT_BLACK); // Solid active dot
        } else {
            _canvas.drawCircle(x, y, 5, TFT_BLACK); // Hollow dot
            _canvas.drawCircle(x, y, 4, TFT_BLACK); // Thickened ring
        }
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
