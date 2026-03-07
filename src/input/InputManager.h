#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/// GPIO used to trigger re-provisioning when held HIGH for 10 s.
static constexpr uint8_t kProvisionPin = 38;

/// Monitors the G38 physical button and provides a touch-based PIN entry UI.
/// The GPIO monitoring runs in its own low-priority FreeRTOS task.
class InputManager {
public:
    static InputManager& getInstance();

    /// Start the background GPIO monitoring task.
    void begin();

    /// @return true if G38 has been held HIGH for ≥10 s since the last clear.
    bool isProvisioningTriggered() const;

    /// Reset the provisioning trigger flag.
    void clearProvisioningTrigger();

    /// Block until the user enters and confirms a valid PIN via touch.
    /// Delegates to DisplayManager::promptPIN().
    /// @return the PIN string entered, or "" on cancellation.
    String waitForPIN(const String& message = "Enter PIN");

    /// Verify a plain-text PIN against the stored SHA-256 hash.
    static bool verifyPIN(const String& pinPlain, const String& pinHash);

private:
    InputManager() = default;

    static void _taskFn(void* param);

    volatile bool _triggered = false;
    TaskHandle_t  _taskHandle = nullptr;
};
