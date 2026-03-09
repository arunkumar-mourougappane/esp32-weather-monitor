/**
 * @file InputManager.h
 * @brief Hardware input handling for the M5Paper physical buttons and touchscreen.
 *
 * InputManager monitors:
 *  - **G38** – re-provisioning trigger button (hold ≥10 s).
 *  - **G37 / G39** – multi-function wheel scroll-up / scroll-down lines.
 *  - **Capacitive touchscreen** – horizontal swipe gestures for forecast scrolling.
 *
 * All GPIO polling and debouncing runs inside a dedicated low-priority FreeRTOS
 * task so the display loop is never blocked.
 */
#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/** @brief GPIO pin connected to the re-provisioning trigger button. */
static constexpr uint8_t kProvisionPin = 38;

/** @brief GPIO pin that fires when the multi-function wheel is scrolled up. */
static constexpr uint8_t kWheelUpPin   = 37;

/** @brief GPIO pin that fires when the multi-function wheel is scrolled down. */
static constexpr uint8_t kWheelDownPin = 39;

/**
 * @class InputManager
 * @brief Singleton that processes all physical and touch input for the device.
 *
 * Gesture detection is performed in real-time during the touch-pressed phase
 * rather than on release, giving immediate visual feedback for swipe scrolling.
 *
 * Typical usage:
 * @code
 *   InputManager::getInstance().begin();
 *   // …in the display loop…
 *   if (InputManager::getInstance().checkSwipeLeft())  --forecastOffset;
 *   if (InputManager::getInstance().checkSwipeRight()) ++forecastOffset;
 * @endcode
 */
class InputManager {
public:
    /**
     * @brief Obtain the singleton instance.
     * @return Reference to the single InputManager object.
     */
    static InputManager& getInstance();

    /**
     * @brief Start the background GPIO/touch monitoring task.
     *
     * Must be called once from setup() before the main loop begins.
     */
    void begin();

    /**
     * @brief Check if the re-provisioning button was held long enough.
     * @return @c true if G38 has been held HIGH for ≥10 s since the last clear.
     */
    bool isProvisioningTriggered() const;

    /**
     * @brief Reset the provisioning trigger flag after it has been handled.
     */
    void clearProvisioningTrigger();

    /**
     * @brief Block the calling task until the user enters a PIN via touch.
     *
     * Delegates rendering to DisplayManager::promptPIN().
     *
     * @param message  Prompt text displayed above the PIN pad.
     * @return         The PIN digit string entered, or @c "" if the user cancelled.
     */
    String waitForPIN(const String& message = "Enter PIN");

    /**
     * @brief Verify a plain-text PIN against a stored SHA-256 hash.
     *
     * Uses mbedTLS to compute the SHA-256 digest of @p pinPlain and compares
     * it (hex-encoded) against @p pinHash.
     *
     * @param pinPlain  Raw PIN string entered by the user.
     * @param pinHash   Hex-encoded SHA-256 hash from ConfigManager.
     * @return          @c true if the digests match.
     */
    static bool verifyPIN(const String& pinPlain, const String& pinHash);

    /**
     * @brief Consume and return the left-swipe flag.
     *
     * Atomically reads and clears the internal flag set when a left swipe
     * exceeding the threshold is detected.
     *
     * @return @c true if a left swipe occurred since the last call.
     */
    bool checkSwipeLeft();

    /**
     * @brief Consume and return the right-swipe flag.
     * @return @c true if a right swipe occurred since the last call.
     */
    bool checkSwipeRight();

    bool checkScrollUp();
    bool checkScrollDown();
    bool checkClick();

private:
    InputManager() = default;

    /** @brief FreeRTOS task entry-point for GPIO and touch polling. */
    static void _taskFn(void* param);

    /** @brief Internal: process raw touch coordinates into swipe events. */
    void _processTouchGestures();

    volatile bool _triggered   = false; ///< Set when G38 hold is detected.
    bool          _isSwiping   = false; ///< True while a touch drag is in progress.
    bool          _swipeLeft   = false; ///< Pending left-swipe event.
    bool          _swipeRight  = false; ///< Pending right-swipe event.
    bool          _scrollUp    = false; ///< Pending jog dial scroll up.
    bool          _scrollDown  = false; ///< Pending jog dial scroll down.
    bool          _click       = false; ///< Pending jog dial click (short press G38).
    int           _touchStartX = 0;     ///< X coordinate where the swipe began.
    int           _touchStartY = 0;     ///< Y coordinate where the swipe began.

    bool _lastWheelUp   = HIGH; ///< Previous state of the wheel-up GPIO (debounce).
    bool _lastWheelDown = HIGH; ///< Previous state of the wheel-down GPIO (debounce).

    TaskHandle_t  _taskHandle = nullptr; ///< Handle for the monitoring task.
};

#endif // INPUT_MANAGER_H
