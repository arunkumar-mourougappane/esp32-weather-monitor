#include "InputManager.h"
#include "../display/DisplayManager.h"
#include <mbedtls/sha256.h>
#include <esp_log.h>

static const char* TAG = "InputManager";

InputManager& InputManager::getInstance() {
    static InputManager instance;
    return instance;
}

void InputManager::begin() {
    pinMode(kProvisionPin, INPUT);

    xTaskCreatePinnedToCore(
        _taskFn,
        "InputTask",
        4096,
        this,
        1,           // low priority
        &_taskHandle,
        0            // core 0
    );
    ESP_LOGI(TAG, "InputTask started on core 0");
}

// ── GPIO38 monitoring task ────────────────────────────────────────────────────
void InputManager::_taskFn(void* param) {
    auto* self = static_cast<InputManager*>(param);
    constexpr uint32_t kPollMs   = 100;
    constexpr uint32_t kHoldMs   = 10000;  // 10 seconds

    uint32_t holdStart = 0;
    bool     holding   = false;

    for (;;) {
        bool pinHigh = digitalRead(kProvisionPin) == HIGH;

        if (pinHigh) {
            if (!holding) {
                holding   = true;
                holdStart = millis();
                ESP_LOGD(TAG, "G38 pressed — hold for 10 s to reprovision");
            } else if (millis() - holdStart >= kHoldMs) {
                ESP_LOGW(TAG, "G38 held 10 s → triggering provisioning");
                self->_triggered = true;
                holding = false; // prevent repeated triggers
            }
        } else {
            holding = false;
        }

        vTaskDelay(pdMS_TO_TICKS(kPollMs));
    }
}

bool InputManager::isProvisioningTriggered() const {
    return _triggered;
}

void InputManager::clearProvisioningTrigger() {
    _triggered = false;
}

// ── PIN entry ─────────────────────────────────────────────────────────────────
String InputManager::waitForPIN(const String& message) {
    return DisplayManager::getInstance().promptPIN(message);
}

// ── PIN verification ──────────────────────────────────────────────────────────
bool InputManager::verifyPIN(const String& pinPlain, const String& pinHash) {
    if (pinHash.isEmpty()) return true; // no PIN set

    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx,
        reinterpret_cast<const uint8_t*>(pinPlain.c_str()), pinPlain.length());
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    String computed;
    computed.reserve(64);
    for (int i = 0; i < 32; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", hash[i]);
        computed += hex;
    }
    return computed.equalsIgnoreCase(pinHash);
}
