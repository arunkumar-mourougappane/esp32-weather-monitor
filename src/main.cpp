#include <M5Unified.h>
#include "config/ConfigManager.h"
#include "input/InputManager.h"
#include "display/DisplayManager.h"
#include "network/WiFiManager.h"
#include "network/NTPManager.h"
#include "provisioning/ProvisioningManager.h"
#include "app/AppController.h"
#include <esp_log.h>

static const char* TAG = "main";

// ── Forward declarations ──────────────────────────────────────────────────────
static bool checkG38AtBoot();
static void runNormalMode(const WeatherConfig& cfg);

// ═════════════════════════════════════════════════════════════════════════════
void setup() {
    // Core M5 init
    auto mcfg = M5.config();
    M5.begin(mcfg);
    Serial.begin(115200);

    // Display
    DisplayManager::getInstance().begin();

    // NVS config
    ConfigManager::getInstance().begin();

    // Input (starts GPIO38 monitoring task immediately)
    InputManager::getInstance().begin();

    bool needProvisioning = !ConfigManager::getInstance().isProvisioned()
                         || checkG38AtBoot();

    if (needProvisioning) {
        ESP_LOGI(TAG, "=== PROVISIONING MODE ===");
        ProvisioningManager::getInstance().begin();
        // Normal flow continues in loop() via ProvisioningManager::run()
        return;
    }

    // ── Normal operation ──────────────────────────────────────────────────
    WeatherConfig cfg = ConfigManager::getInstance().load();

    // PIN gate — show PIN pad until correct PIN entered
    if (!cfg.pin_hash.isEmpty()) {
        bool unlocked = false;
        while (!unlocked) {
            String entered = InputManager::getInstance().waitForPIN();
            unlocked = InputManager::verifyPIN(entered, cfg.pin_hash);
            if (!unlocked) {
                DisplayManager::getInstance()
                    .showMessage("Wrong PIN", "Try again");
                delay(1500);
            }
        }
    }

    runNormalMode(cfg);
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    M5.update();

    // In provisioning mode, pump the provisioning manager
    if (!ConfigManager::getInstance().isProvisioned()) {
        ProvisioningManager::getInstance().run();
    }
    // In normal mode all work happens inside AppController FreeRTOS tasks —
    // loop() just keeps the watchdog fed.
}

// ═════════════════════════════════════════════════════════════════════════════
/// Returns true if G38 is pulled LOW at startup (debounced 100 ms sample).
static bool checkG38AtBoot() {
    pinMode(kProvisionPin, INPUT_PULLUP);
    constexpr uint32_t kSampleMs = 100;
    delay(kSampleMs);
    bool held = (digitalRead(kProvisionPin) == LOW);
    if (held) ESP_LOGW(TAG, "G38 LOW at boot → provisioning");
    return held;
}

/// Connect WiFi, sync NTP, then start all AppController tasks.
static void runNormalMode(const WeatherConfig& cfg) {
    ESP_LOGI(TAG, "=== NORMAL MODE ===");

    DisplayManager::getInstance()
        .showMessage("Connecting...", cfg.wifi_ssid);

    bool connected = WiFiManager::getInstance()
                         .connectSTA(cfg.wifi_ssid, cfg.wifi_pass);
    if (!connected) {
        DisplayManager::getInstance()
            .showMessage("WiFi Failed", "Check credentials — will retry");
        // AppController WeatherTask will retry on its next cycle
    } else {
        NTPManager::getInstance().sync(cfg.ntp_server, cfg.timezone);
    }

    AppController::getInstance().begin();
}
