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
void DisplayManager::_drawQR(const String& url, int ox, int oy, int moduleSize) {
    // Version 5, ECC_LOW → up to 106 alphanumeric chars
    QRCode qrcode;
    uint8_t buf[qrcode_getBufferSize(5)];
    qrcode_initText(&qrcode, buf, 5, ECC_LOW, url.c_str());

    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            uint32_t color = qrcode_getModule(&qrcode, x, y)
                             ? TFT_BLACK : TFT_WHITE;
            M5.Display.fillRect(ox + x * moduleSize,
                                oy + y * moduleSize,
                                moduleSize, moduleSize, color);
        }
    }
}

void DisplayManager::showProvisioningScreen(const String& ssid,
                                            const String& apUrl) {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    clear();

    constexpr int moduleSize = 6;           // 37 modules × 6 = 222 px
    constexpr int qrSizePx   = 37 * moduleSize; // ~222
    constexpr int qrOX = (kWidth  - qrSizePx) / 2;
    constexpr int qrOY = 140;

    // ── Title ───────────────────────────────────────────────────
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString("Scan to Configure", kWidth / 2, 36, 1);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("Connect to WiFi below, then open the URL",
                                kWidth / 2, 74, 1);

    // ── QR Code ─────────────────────────────────────────────────
    // Draw white border around QR for quiet zone
    M5.Display.fillRect(qrOX - 12, qrOY - 12,
                        qrSizePx + 24, qrSizePx + 24, TFT_WHITE);
    _drawQR(apUrl, qrOX, qrOY, moduleSize);

    // ── Caption ─────────────────────────────────────────────────
    int captionY = qrOY + qrSizePx + 36;
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString("WiFi: " + ssid, kWidth / 2, captionY, 1);
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(apUrl, kWidth / 2, captionY + 44, 1);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString("No password required", kWidth / 2, captionY + 82, 1);

    ESP_LOGI(TAG, "Provisioning screen shown (URL: %s)", apUrl.c_str());
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
                                   const String& city) {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    clear();

    // Date / time header
    char timeBuf[32], dateBuf[48];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
    strftime(dateBuf,  sizeof(dateBuf),  "%A, %B %d %Y", &t);

    M5.Display.setTextSize(3);
    M5.Display.drawCentreString(timeBuf, kWidth / 2, 40, 1);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString(dateBuf, kWidth / 2, 96, 1);

    // Divider
    M5.Display.drawFastHLine(30, 120, kWidth - 60, TFT_DARKGREY);

    // City
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(city, kWidth / 2, 136, 1);

    if (!data.valid) {
        M5.Display.setTextSize(1);
        M5.Display.drawCentreString("Fetching weather...", kWidth / 2, 220, 1);
        return;
    }

    // Temperature
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f°C", data.tempC);
    M5.Display.setTextSize(5);
    M5.Display.drawCentreString(tempBuf, kWidth / 2, 190, 1);

    // Condition
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(data.condition, kWidth / 2, 290, 1);

    // Details row
    M5.Display.setTextSize(1);
    char details[80];
    snprintf(details, sizeof(details),
             "Feels %.1f°C  |  Humidity %d%%  |  Wind %.0f km/h",
             data.feelsLikeC, data.humidity, data.windKph);
    M5.Display.drawCentreString(details, kWidth / 2, 340, 1);

    // UV index
    char uvBuf[24];
    snprintf(uvBuf, sizeof(uvBuf), "UV Index: %d", data.uvIndex);
    M5.Display.drawCentreString(uvBuf, kWidth / 2, 364, 1);
}

void DisplayManager::showMessage(const String& title, const String& body) {
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    clear();
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(title, kWidth / 2, 200, 1);
    M5.Display.setTextSize(1);
    M5.Display.drawCentreString(body, kWidth / 2, 260, 1);
}
