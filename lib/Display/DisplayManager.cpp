#include "DisplayManager.h"
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
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
}

void DisplayManager::clear() {
    M5.Display.fillScreen(TFT_WHITE);
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


// ── Weather placeholder UI ────────────────────────────────────────────────────
void DisplayManager::showWeatherUI(const WeatherData& data,
                                   const struct tm& t,
                                   const String& city,
                                   bool fastMode,
                                   int forecastOffset) {
    M5.Display.setEpdMode(fastMode ? epd_mode_t::epd_fastest
                                   : epd_mode_t::epd_quality);
    if (!fastMode) clear();

    _drawBattery();

    // Date / time header
    char timeBuf[32], dateBuf[48];
    strftime(timeBuf, sizeof(timeBuf), "%l:%M %p", &t);
    strftime(dateBuf,  sizeof(dateBuf),  "%A, %B %d %Y", &t);

    M5.Display.setFont(&fonts::FreeSansBold24pt7b);
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(timeBuf, kWidth / 2, 60);

    M5.Display.setFont(&fonts::FreeSans18pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString(dateBuf, kWidth / 2, 180);

    // Divider
    M5.Display.drawFastHLine(40, 230, kWidth - 80, TFT_DARKGREY);

    // City
    M5.Display.setFont(&fonts::FreeSans24pt7b);
    M5.Display.drawCentreString(city, kWidth / 2, 270);

    if (!data.valid) {
        M5.Display.setFont(&fonts::FreeSans18pt7b);
        M5.Display.drawCentreString("Fetching weather...", kWidth / 2, 480);
        return;
    }

    // Temperature
    M5.Display.setFont(&fonts::FreeSansBold24pt7b);
    M5.Display.setTextSize(2);
    char tempBuf[32];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f C", data.tempC);
    M5.Display.drawCentreString(tempBuf, kWidth / 2, 360);

    // Condition
    M5.Display.setFont(&fonts::FreeSans24pt7b);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString(data.condition, kWidth / 2, 480);

    // Details Grid
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    char buf1[32], buf2[32];
    
    snprintf(buf1, sizeof(buf1), "Feels: %.1f C", data.feelsLikeC);
    M5.Display.drawString(buf1, 60, 560);
    snprintf(buf2, sizeof(buf2), "Wind: %.0f km/h", data.windKph);
    M5.Display.drawString(buf2, kWidth/2 + 20, 560);

    snprintf(buf1, sizeof(buf1), "Humidity: %d%%", data.humidity);
    M5.Display.drawString(buf1, 60, 600);
    snprintf(buf2, sizeof(buf2), "UV Index: %d", data.uvIndex);
    M5.Display.drawString(buf2, kWidth/2 + 20, 600);

    snprintf(buf1, sizeof(buf1), "Clouds: %d%%", data.cloudCover);
    M5.Display.drawString(buf1, 60, 640);
    snprintf(buf2, sizeof(buf2), "Vis: %.1f km", data.visibilityKm);
    M5.Display.drawString(buf2, kWidth/2 + 20, 640);

    // ── Forecast Section (Bottom) ─────────────────────────────────────────────
    // Divider
    M5.Display.drawFastHLine(20, 690, kWidth - 40, TFT_DARKGREY);

    // Render the forecast portion
    updateForecastUI(data, forecastOffset);
    
    // Reset font for other screens
    M5.Display.setFont(nullptr);
}

void DisplayManager::updateForecastUI(const WeatherData& data, int forecastOffset) {
    // Clear only the bottom forecast region (below the divider at Y=690)
    M5.Display.fillRect(0, 691, kWidth, kHeight - 691, TFT_WHITE);
    M5.Display.setEpdMode(epd_mode_t::epd_fast); // Use fast mode for partial updates to avoid flicker
    
    if (data.forecastDays > 0) {
        int maxItems = std::min(3, data.forecastDays - forecastOffset);
        int itemWidth = kWidth / 3;

        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.setTextSize(1);
        
        for (int i = 0; i < maxItems; i++) {
            int idx = forecastOffset + i;
            if (idx >= 5) break;

            const auto& f = data.forecast[idx];
            int cx = (i * itemWidth) + (itemWidth / 2);

            // Print Day offset roughly (Today, +1, +2, etc)
            String dayLabel = (idx == 0) ? "Today" : ("Day " + String(idx+1));
            
            // Render forecast item
            M5.Display.drawCentreString(dayLabel, cx, 720);
            
            // Condition (truncate if too long)
            String cond = f.condition;
            if (cond.length() > 9) cond = cond.substring(0, 7) + "..";
            M5.Display.drawCentreString(cond, cx, 760);
            
            // High / Low temps
            M5.Display.setFont(&fonts::FreeSans9pt7b);
            char tempRange[32];
            snprintf(tempRange, sizeof(tempRange), "H:%0.f L:%0.f", f.maxTempC, f.minTempC);
            M5.Display.drawCentreString(tempRange, cx, 800);
            
            // Precip chance
            if (f.precipChance > 0) {
                M5.Display.setFont(&fonts::FreeSans9pt7b);
                char pop[16];
                snprintf(pop, sizeof(pop), "Drop: %d%%", f.precipChance);
                M5.Display.drawCentreString(pop, cx, 830);
            }
            M5.Display.setFont(&fonts::FreeSans12pt7b);
        }
        
        // Draw Scroll indicators
        if (forecastOffset > 0) {
            M5.Display.fillTriangle(10, 810, 30, 790, 30, 830, TFT_BLACK);
        }
        if (forecastOffset + 3 < data.forecastDays) {
            M5.Display.fillTriangle(kWidth-10, 810, kWidth-30, 790, kWidth-30, 830, TFT_BLACK);
        }
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
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    clear();

    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // City name at top
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(city, kWidth / 2, 120, 1);

    // Large status text
    M5.Display.setTextSize(3);
    M5.Display.drawCentreString("Fetching weather", kWidth / 2, 420, 1);

    // Dot indicator
    M5.Display.setTextSize(4);
    M5.Display.drawCentreString(". . .", kWidth / 2, 520, 1);

    ESP_LOGI("DisplayManager", "Loading screen shown for city: %s", city.c_str());
}

void DisplayManager::_drawBattery() {
    // M5Paper uses a 1/2 voltage divider on GPIO 35 for battery ADC.
    // M5Unified's adc_oneshot relies on a clock source that is broken in the
    // current ESP-IDF version for this board. We bypass it using stable Arduino methods.
    int32_t pin_mv = analogReadMilliVolts(35);
    int32_t mv = pin_mv * 2;
    ESP_LOGI(TAG, "Battery Voltage: %ld mV (Pin 35: %ld mV)", mv, pin_mv);

    // Standard 1S LiPo logic: ~4100mV is 100%, ~3200mV is 0%
    int level = 0;
    if (mv >= 4100) {
        level = 100;
    } else if (mv <= 3200) {
        level = 0;
    } else {
        level = (int)(((mv - 3200) * 100) / (4100 - 3200));
    }

    int x = kWidth - 55;
    int y = 15;

    // Erase bounding box for fast-refresh overwrites (pure white background)
    M5.Display.fillRect(x - 45, y, 100, 20, TFT_WHITE);

    // Bounding outer cell
    M5.Display.drawRoundRect(x, y, 40, 20, 3, TFT_BLACK);
    // Positive terminal nub
    M5.Display.fillRect(x + 40, y + 5, 4, 10, TFT_BLACK);
    
    // Internal fill calculation
    int fillW = (36 * level) / 100;
    if (fillW > 0) {
        M5.Display.fillRect(x + 2, y + 2, fillW, 16, TFT_BLACK);
    }
    
    // Readout percentage text aligned dynamically to the left of the battery
    M5.Display.setTextDatum(MR_DATUM);
    M5.Display.setTextSize(1);
    M5.Display.setFont(nullptr);
    M5.Display.drawString(String(level) + "%", x - 5, y + 10);
    // Reset alignment
    M5.Display.setTextDatum(TL_DATUM);
}
