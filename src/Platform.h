// ---------------------------------------------------------------------------
// Loom — Platform Abstraction
// ---------------------------------------------------------------------------
// Provides platform-specific macros and inline helpers. Currently targets
// nRF52840 (Adafruit Bluefruit BSP) exclusively. A future port would swap
// this header or use conditional compilation.
// ---------------------------------------------------------------------------

#ifndef LOOM_PLATFORM_H
#define LOOM_PLATFORM_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>

// ---------------------------------------------------------------------------
// Configurable defaults (user may #define before including Loom.h)
// ---------------------------------------------------------------------------

#ifndef LOOM_DEFAULT_STACK_SIZE
#define LOOM_DEFAULT_STACK_SIZE  512   // bytes (128 words on 32-bit ARM)
#endif

#ifndef LOOM_MAX_TIMERS_PER_ACTOR
#define LOOM_MAX_TIMERS_PER_ACTOR  4
#endif

#ifndef LOOM_MAX_ACTORS
#define LOOM_MAX_ACTORS  16
#endif

#ifndef LOOM_MAX_SUBSCRIPTIONS
#define LOOM_MAX_SUBSCRIPTIONS  64
#endif

#ifndef LOOM_SLOW_HANDLER_THRESHOLD_MS
#define LOOM_SLOW_HANDLER_THRESHOLD_MS  10
#endif

// ---------------------------------------------------------------------------
// Debug macros
// ---------------------------------------------------------------------------
// All debug output compiles to nothing when LOOM_DEBUG_ENABLED is not defined.
// When enabled, output goes through Serial.printf (available on ARM/nRF52).
// ---------------------------------------------------------------------------

#ifdef LOOM_DEBUG_ENABLED

#define LOOM_LOG(fmt, ...)      do { Serial.printf("[Loom] " fmt "\r\n", ##__VA_ARGS__); } while (0)
#define LOOM_WARN(fmt, ...)     do { Serial.printf("[Loom] WARNING: " fmt "\r\n", ##__VA_ARGS__); } while (0)

#else

#define LOOM_LOG(fmt, ...)      ((void)0)
#define LOOM_WARN(fmt, ...)     ((void)0)

#endif // LOOM_DEBUG_ENABLED

// ---------------------------------------------------------------------------
// Timing helpers
// ---------------------------------------------------------------------------

inline uint32_t loomMillis() {
    return (uint32_t)millis();
}

inline TickType_t loomMsToTicks(uint32_t ms) {
    return pdMS_TO_TICKS(ms);
}

// ---------------------------------------------------------------------------
// Stack size helper — converts bytes to words (StackType_t units)
// ---------------------------------------------------------------------------

constexpr uint32_t loomStackWords(uint32_t bytes) {
    return bytes / sizeof(StackType_t);
}

#endif // LOOM_PLATFORM_H
