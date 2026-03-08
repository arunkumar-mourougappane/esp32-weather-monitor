#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/// GPIO used to trigger re-provisioning when held HIGH for 10 s.
static constexpr uint8_t kProvisionPin = 38;

/// Multi-function wheel scroll pins
static constexpr uint8_t kWheelUpPin   = 37;
static constexpr uint8_t kWheelDownPin = 39;

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

    /// Check if a completed left-swipe gesture was detected (clears the flag).
    bool checkSwipeLeft();

    /// Check if a completed right-swipe gesture was detected (clears the flag).
    bool checkSwipeRight();

private:
    InputManager() = default;

    static void _taskFn(void* param);
    void _processTouchGestures();

    volatile bool _triggered   = false;
    bool          _isSwiping   = false;
    bool          _swipeLeft   = false;
    bool          _swipeRight  = false;
    int           _touchStartX = 0;
    int           _touchStartY = 0;
    
    // Wheel state tracking for debouncing
    bool          _lastWheelUp   = HIGH;
    bool          _lastWheelDown = HIGH;
    
    TaskHandle_t  _taskHandle = nullptr;
};
