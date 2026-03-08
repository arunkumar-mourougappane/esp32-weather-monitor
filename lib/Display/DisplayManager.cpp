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
    _canvas.setColorDepth(4);
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


// ── Weather placeholder UI ────────────────────────────────────────────────────
void DisplayManager::showWeatherUI(const WeatherData& data,
                                   const struct tm& t,
                                   const String& city,
                                   bool fastMode,
                                   int forecastOffset) {
    M5.Display.setEpdMode(fastMode ? epd_mode_t::epd_fastest
                                   : epd_mode_t::epd_quality);
    if (!fastMode) clear();
    _canvas.fillSprite(TFT_WHITE);

    _drawBattery();

    // Date / time header
    char timeBuf[32], dateBuf[48];
    strftime(timeBuf, sizeof(timeBuf), "%l:%M %p", &t);
    strftime(dateBuf,  sizeof(dateBuf),  "%A, %B %d %Y", &t);

    _canvas.setFont(&fonts::FreeSansBold24pt7b);
    _canvas.setTextSize(2);
    _canvas.drawCentreString(timeBuf, kWidth / 2, 60);

    // ── Main Content ──────────────────────────────────────────────────────────
    String dispCity = city;
    if (dispCity.isEmpty()) dispCity = "Unknown";

    // City
    _canvas.setFont(&fonts::FreeSans24pt7b);
    _canvas.setTextSize(1);
    _canvas.drawCentreString(dispCity, kWidth / 2, 80);
    
    // Date
    _canvas.setFont(&fonts::FreeSans18pt7b);
    _canvas.drawCentreString(dateBuf, kWidth / 2, 130);
    
    // Divider
    _canvas.drawFastHLine(20, 170, kWidth - 40, TFT_BLACK);

    if (!data.valid) {
        _canvas.setFont(&fonts::FreeSans18pt7b);
        _canvas.drawCentreString("Fetching weather...", kWidth / 2, 480);
        return;
    }

    // ── Hero Section (Icon + Temp) ────────────────────────────────────────────
    _drawWeatherIcon(data.condition, 160, 240, 40); // 40px radius icon at X=160
    
    _canvas.setFont(&fonts::FreeSansBold24pt7b);
    _canvas.setTextSize(2); // Massive temperature
    char tempBuf[32];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f C", data.tempC);
    _canvas.drawString(tempBuf, 250, 200); // Draw starting at X=250

    // Condition String
    _canvas.setFont(&fonts::FreeSans24pt7b);
    _canvas.setTextSize(1);
    _canvas.drawCentreString(data.condition, kWidth / 2, 330);

    // ── Details Grid ──────────────────────────────────────────────────────────
    _canvas.setFont(&fonts::FreeSans12pt7b);
    char buf1[32], buf2[32];
    
    snprintf(buf1, sizeof(buf1), "Feels: %.1f C", data.feelsLikeC);
    _canvas.drawString(buf1, 40, 390);
    snprintf(buf2, sizeof(buf2), "Wind: %.0f km/h", data.windKph);
    _canvas.drawString(buf2, kWidth/2 + 20, 390);

    snprintf(buf1, sizeof(buf1), "Hum: %d%%", data.humidity);
    _canvas.drawString(buf1, 40, 430);
    snprintf(buf2, sizeof(buf2), "Clouds: %d%%", data.cloudCover);
    _canvas.drawString(buf2, kWidth/2 + 20, 430);

    // ── Environmental Dials ───────────────────────────────────────────────────
    _drawAQIGauge(data.aqi, 510);
    _drawSunArc(time(nullptr), data.sunriseTime, data.sunsetTime, 510);
    
    // Divider
    _canvas.drawFastHLine(20, 560, kWidth - 40, TFT_BLACK);

    // ── 10-Day Sparkline ──────────────────────────────────────────────────────
    _drawForecastSparkline(data, 600);
    
    _canvas.drawFastHLine(20, 690, kWidth - 40, TFT_BLACK);

    // Render the forecast portion
    updateForecastUI(data, forecastOffset);
    
    // Reset font for other screens
    _canvas.setFont(nullptr);
    _canvas.pushSprite(0, 0);
}

void DisplayManager::updateForecastUI(const WeatherData& data, int forecastOffset) {
    // Clear only the bottom forecast region (below the divider at Y=690)
    _canvas.fillRect(0, 691, kWidth, kHeight - 691, TFT_WHITE);
    M5.Display.setEpdMode(epd_mode_t::epd_fast); // Use fast mode for partial updates to avoid flicker
    
    if (data.forecastDays > 0) {
        int maxItems = std::min(3, data.forecastDays - forecastOffset);
        int itemWidth = kWidth / 3;

        _canvas.setFont(&fonts::FreeSans12pt7b);
        _canvas.setTextSize(1);
        
        for (int i = 0; i < maxItems; i++) {
            int idx = forecastOffset + i;
            if (idx >= 5) break;

            const auto& f = data.forecast[idx];
            int cx = (i * itemWidth) + (itemWidth / 2);

            // Print Day offset roughly (Today, +1, +2, etc)
            String dayLabel = (idx == 0) ? "Today" : ("Day " + String(idx+1));
            
            // Render forecast item
            _canvas.drawCentreString(dayLabel, cx, 720);
            
            // Condition (truncate if too long)
            String cond = f.condition;
            if (cond.length() > 9) cond = cond.substring(0, 7) + "..";
            _canvas.drawCentreString(cond, cx, 760);
            
            // High / Low temps
            _canvas.setFont(&fonts::FreeSans9pt7b);
            char tempRange[32];
            snprintf(tempRange, sizeof(tempRange), "H:%0.f L:%0.f", f.maxTempC, f.minTempC);
            _canvas.drawCentreString(tempRange, cx, 800);
            
            // Precip chance
            if (f.precipChance > 0) {
                _canvas.setFont(&fonts::FreeSans9pt7b);
                char pop[16];
                snprintf(pop, sizeof(pop), "Drop: %d%%", f.precipChance);
                _canvas.drawCentreString(pop, cx, 830);
            }
            _canvas.setFont(&fonts::FreeSans12pt7b);
        }
        
        // Draw Scroll indicators
        if (forecastOffset > 0) {
            _canvas.fillTriangle(10, 810, 30, 790, 30, 830, TFT_BLACK);
        }
        if (forecastOffset + 3 < data.forecastDays) {
            _canvas.fillTriangle(kWidth-10, 810, kWidth-30, 790, kWidth-30, 830, TFT_BLACK);
        }
    }
    _canvas.pushSprite(0, 0);
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
    _canvas.fillRect(x - 45, y, 100, 20, TFT_WHITE);

    // Bounding outer cell
    _canvas.drawRoundRect(x, y, 40, 20, 3, TFT_BLACK);
    // Positive terminal nub
    _canvas.fillRect(x + 40, y + 5, 4, 10, TFT_BLACK);
    
    // Internal fill calculation
    int fillW = (36 * level) / 100;
    if (fillW > 0) {
        _canvas.fillRect(x + 2, y + 2, fillW, 16, TFT_BLACK);
    }
    
    // Readout percentage text aligned dynamically to the left of the battery
    _canvas.setTextDatum(MR_DATUM);
    _canvas.setTextSize(1);
    _canvas.setFont(nullptr);
    _canvas.drawString(String(level) + "%", x - 5, y + 10);
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

void DisplayManager::_drawForecastSparkline(const WeatherData& data, int yOff) {
    if (data.forecastDays < 2) return;
    
    int padding = 60;
    int chartW = kWidth - (padding * 2);
    int chartH = 60;
    
    // Find absolute Min and Max across the graph boundaries
    float minT = data.forecast[0].minTempC;
    float maxT = data.forecast[0].maxTempC;
    for (int i = 0; i < data.forecastDays; i++) {
        if (data.forecast[i].minTempC < minT) minT = data.forecast[i].minTempC;
        if (data.forecast[i].maxTempC > maxT) maxT = data.forecast[i].maxTempC;
    }
    float range = maxT - minT;
    if (range < 1.0f) range = 1.0f; // Division safety
    
    // Draw Axis lines
    _canvas.drawFastHLine(padding - 10, yOff + chartH, chartW + 20, TFT_BLACK);
    
    // Y-Axis Min/Max Labels
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.setTextDatum(MR_DATUM);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f\xf8", maxT);
    _canvas.drawString(buf, padding - 15, yOff);
    snprintf(buf, sizeof(buf), "%.0f\xf8", minT);
    _canvas.drawString(buf, padding - 15, yOff + chartH);
    _canvas.setTextDatum(TL_DATUM);
    
    int stepX = chartW / (data.forecastDays - 1);
    int prevMaxX = -1, prevMaxY = -1;
    
    for (int i = 0; i < data.forecastDays; i++) {
        int x = padding + (i * stepX);
        int maxY = yOff + chartH - (int)(((data.forecast[i].maxTempC - minT) / range) * chartH);
        
        // Data tick
        _canvas.fillCircle(x, maxY, 4, TFT_BLACK);
        
        if (prevMaxX != -1) {
            // Thick sparkline
            _canvas.drawLine(prevMaxX, prevMaxY, x, maxY, TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY-1, x, maxY-1, TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY+1, x, maxY+1, TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY-2, x, maxY-2, TFT_BLACK);
            _canvas.drawLine(prevMaxX, prevMaxY+2, x, maxY+2, TFT_BLACK);
        }
        
        prevMaxX = x; prevMaxY = maxY;
        // X-Axis day tick
        _canvas.drawLine(x, yOff + chartH, x, yOff + chartH + 5, TFT_BLACK);
        _canvas.drawLine(x+1, yOff + chartH, x+1, yOff + chartH + 5, TFT_BLACK);
    }
    
    // Graph Title
    _canvas.setFont(&fonts::FreeSansBold9pt7b);
    _canvas.drawCentreString("10-Day Trend", kWidth/2, yOff - 25);
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
