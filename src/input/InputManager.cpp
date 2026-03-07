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
    pinMode(kProvisionPin, INPUT_PULLUP);
    pinMode(kWheelUpPin,   INPUT_PULLUP);
    pinMode(kWheelDownPin, INPUT_PULLUP);

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
        bool pinPulledLow = digitalRead(kProvisionPin) == LOW;

        if (pinPulledLow) {
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

        self->_processTouchGestures();

        vTaskDelay(pdMS_TO_TICKS(kPollMs));
    }
}

bool InputManager::isProvisioningTriggered() const {
    return _triggered;
}

void InputManager::clearProvisioningTrigger() {
    _triggered = false;
}

// ── Touch swipe detection ─────────────────────────────────────────────────────
bool InputManager::checkSwipeLeft() {
    bool result = _swipeLeft;
    _swipeLeft = false;
    return result;
}

bool InputManager::checkSwipeRight() {
    bool result = _swipeRight;
    _swipeRight = false;
    return result;
}

void InputManager::_processTouchGestures() {
    M5.update();
    
    // Check wheel roll actions (active LOW)
    bool currentUp   = digitalRead(kWheelUpPin);
    bool currentDown = digitalRead(kWheelDownPin);
    
    // Detect falling edge (HIGH to LOW)
    if (_lastWheelUp == HIGH && currentUp == LOW) {
        _swipeLeft = true;
        ESP_LOGI(TAG, "Wheel Left/Up Detected (G37)");
    }
    if (_lastWheelDown == HIGH && currentDown == LOW) {
        _swipeRight = true;
        ESP_LOGI(TAG, "Wheel Right/Down Detected (G39)");
    }
    
    _lastWheelUp   = currentUp;
    _lastWheelDown = currentDown;

    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) {
            _touchStartX = t.x;
            _touchStartY = t.y;
            _isSwiping = true;
        } else if (t.wasReleased() && _isSwiping) {
            int dx = t.x - _touchStartX;
            int dy = t.y - _touchStartY;
            
            // If horizontal movement is significant and greater than vertical movement
            if (abs(dx) > 50 && abs(dx) > abs(dy)) {
                if (dx < 0) {
                    _swipeLeft = true;
                    ESP_LOGI(TAG, "Swipe Left Detected");
                } else {
                    _swipeRight = true;
                    ESP_LOGI(TAG, "Swipe Right Detected");
                }
            }
            _isSwiping = false;
        }
    }
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
