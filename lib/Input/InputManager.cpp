#include "InputManager.h"
#include <M5Unified.h>
#include <EventBus.h>
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
    constexpr uint32_t kPollMs    = 20;     // 50Hz polling catches tactile rotary clicks & smooths touch swipes
    constexpr uint32_t kHoldMs    = 10000;  // 10 seconds → provisioning
    constexpr uint32_t kLongPress = 2000;   // 2 seconds → long-press action

    uint32_t holdStart = 0;
    bool     holding   = false;

    for (;;) {
        bool pinPulledLow = digitalRead(kProvisionPin) == LOW;

        if (pinPulledLow) {
            if (!holding) {
                holding   = true;
                holdStart = millis();
                ESP_LOGD(TAG, "G38 pressed — hold for 10 s to reprovision, short click for select");
            } else if (millis() - holdStart >= kHoldMs) {
                ESP_LOGW(TAG, "G38 held 10 s → triggering provisioning");
                self->_triggered = true;
                holding = false; // prevent repeated triggers
            } else if (!self->_longPress && (millis() - holdStart >= kLongPress)) {
                self->_longPress = true;
                ESP_LOGI(TAG, "G38 long-press (2 s) detected");
            }
        } else {
            // Short press detection (less than 2 seconds)
            if (holding && (millis() - holdStart < kLongPress)) {
                self->_click++;
                ESP_LOGI(TAG, "Jog Dial Click Detected (G38 Short Press), count: %d", self->_click);
            }
            holding = false;
        }

        // Touch/button processing (M5.update) is handled by pollInput() on the
        // main loop to avoid I2C contention with RTC and other peripherals.

        vTaskDelay(pdMS_TO_TICKS(kPollMs));
    }
}

// ── Public poll — call once per main-loop tick ───────────────────────────────
void InputManager::pollInput() {
    _processTouchGestures();
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

bool InputManager::checkSwipeUp() {
    bool result = _swipeUp;
    _swipeUp = false;
    return result;
}

bool InputManager::checkSwipeDown() {
    bool result = _swipeDown;
    _swipeDown = false;
    return result;
}

int InputManager::checkScrollUp() {
    int result = _scrollUp;
    _scrollUp = 0;
    return result;
}

int InputManager::checkScrollDown() {
    int result = _scrollDown;
    _scrollDown = 0;
    return result;
}

int InputManager::checkClick() {
    int result = _click;
    _click = 0;
    return result;
}

bool InputManager::checkLongPress() {
    bool result = _longPress;
    _longPress = false;
    return result;
}

bool InputManager::checkTap(int& x, int& y) {
    if (_tapX >= 0) {
        x = _tapX;
        y = _tapY;
        _tapX = -1;
        _tapY = -1;
        return true;
    }
    return false;
}

void InputManager::_processTouchGestures() {
    M5.update();
    
    // Check M5Unified native hardware-debounced jog dial triggers
    if (M5.BtnA.wasClicked() || M5.BtnA.wasPressed()) {
        _scrollUp++;
        ESP_LOGI(TAG, "Wheel Up Detected (BtnA), count: %d", _scrollUp);
    }
    if (M5.BtnC.wasClicked() || M5.BtnC.wasPressed()) {
        _scrollDown++;
        ESP_LOGI(TAG, "Wheel Down Detected (BtnC), count: %d", _scrollDown);
    }

    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) {
            _touchStartX = t.x;
            _touchStartY = t.y;
            _isSwiping = true;
        } else if (_isSwiping && (t.isPressed() || t.wasReleased())) {
            int dx = t.x - _touchStartX;
            int dy = t.y - _touchStartY;
            
            // Trigger immediately once 30px threshold is crossed (faster response)
            if (abs(dx) > 30 && abs(dx) > abs(dy)) {
                if (dx < 0) {
                    _swipeLeft = true;
                    ESP_LOGI(TAG, "Swipe Left Detected");
                } else {
                    _swipeRight = true;
                    ESP_LOGI(TAG, "Swipe Right Detected");
                }
                _isSwiping = false; // Prevent multiple triggers in same gesture
            } else if (abs(dy) > 30 && abs(dy) > abs(dx)) {
                // Vertical swipes added
                if (dy < 0) {
                    _swipeUp = true;
                    ESP_LOGI(TAG, "Swipe Up Detected");
                } else {
                    _swipeDown = true;
                    ESP_LOGI(TAG, "Swipe Down Detected");
                }
                _isSwiping = false;
            } else if (t.wasReleased()) {
                // Released without crossing swipe threshold — record as a tap
                _tapX = _touchStartX;
                _tapY = _touchStartY;
                ESP_LOGI(TAG, "Tap at (%d, %d)", _tapX, _tapY);
                _isSwiping = false;
            }
        }
    }
}

// ── PIN entry ─────────────────────────────────────────────────────────────────
String InputManager::waitForPIN(const String& message) {
    PinPromptPayload payload{ message.c_str(), "" };
    EventBus::publish(SystemEvent::EV_PIN_PROMPT_REQUESTED, &payload);
    return payload.result;
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
