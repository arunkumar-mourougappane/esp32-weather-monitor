#include <M5Unified.h>
#include <ConfigManager.h>
#include <InputManager.h>
#include <DisplayManager.h>
#include <WiFiManager.h>
#include <NTPManager.h>
#include <ProvisioningManager.h>
#include <AppController.h>
#include <esp_log.h>

static const char* TAG = "main";

// ── Forward declarations ──────────────────────────────────────────────────────
static bool checkG38AtBoot();
static void runNormalMode(const WeatherConfig& cfg);
static void _appTask(void* arg);

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

    WeatherConfig cfg = ConfigManager::getInstance().load();

    bool forceProvisioning = ConfigManager::getInstance().isForceProvisioning();
    bool needProvisioning = !ConfigManager::getInstance().isProvisioned()
                         || checkG38AtBoot()
                         || forceProvisioning;

    if (needProvisioning) {
        if (forceProvisioning) {
            ConfigManager::getInstance().setForceProvisioning(false);
        }

        // PIN gate — show PIN pad until correct PIN entered if a PIN is set.
        // After 3 consecutive wrong PINs the device reboots to prevent brute-force.
        if (!cfg.pin_hash.isEmpty()) {
            bool unlocked = false;
            int attempts = 0;
            while (!unlocked) {
                String entered = InputManager::getInstance().waitForPIN("Unlock Provisioning");
                unlocked = InputManager::verifyPIN(entered, cfg.pin_hash);
                if (!unlocked) {
                    attempts++;
                    if (attempts >= 3) {
                        DisplayManager::getInstance()
                            .showMessage("Too Many Attempts", "Rebooting device...");
                        delay(2000);
                        esp_restart();
                    }
                    DisplayManager::getInstance()
                        .showMessage("Wrong PIN", "Try again");
                    delay(1500);
                }
            }
        }

        ESP_LOGI(TAG, "=== PROVISIONING MODE ===");
        ProvisioningManager::getInstance().begin();
        // Normal flow continues in loop() via ProvisioningManager::run()
        return;
    }

    // ── Normal operation ──────────────────────────────────────────────────
    // loopTask has a fixed 8 KB stack. The mbedTLS TLS handshake alone needs
    // ~8-10 KB, so running the fetch path directly in setup() overflows the
    // stack canary ("Stack canary watchpoint triggered (loopTask)").  Spawn a
    // dedicated task with a 24 KB stack instead; setup()/loop() stay idle.
    xTaskCreatePinnedToCore(_appTask, "appTask", 24576, nullptr, 5, nullptr, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // InputTask (core 0) owns M5.update() — calling it here from loopTask
    // (core 1) as well races against InputTask and clears wasPressed/wasReleased
    // edge flags before they can be read, silently dropping all touch events.

    // In provisioning mode, pump the provisioning manager
    if (!ConfigManager::getInstance().isProvisioned()) {
        ProvisioningManager::getInstance().run();
    }
    // In normal mode all work happens inside AppController FreeRTOS tasks —
    // loop() just keeps the watchdog fed.
    delay(10);
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

/// Passes control directly to AppController which manages Wakeup reasons.
static void runNormalMode(const WeatherConfig& cfg) {
    ESP_LOGI(TAG, "=== NORMAL MODE ===");
    AppController::getInstance().begin();
}

/// FreeRTOS task wrapper that runs the normal-mode app logic with an adequate
/// stack (24 KB).  The task ends with esp_deep_sleep_start() inside
/// AppController::begin() and never reaches vTaskDelete().
static void _appTask(void* /*arg*/) {
    WeatherConfig cfg = ConfigManager::getInstance().load();
    runNormalMode(cfg);
    vTaskDelete(nullptr);
}
