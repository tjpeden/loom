// ---------------------------------------------------------------------------
// Loom — Event & Signal Definitions
// ---------------------------------------------------------------------------
// An Event is a small value type: a uint16_t signal plus a 32-byte payload.
// Events are copied by value into FreeRTOS queues — no heap allocation.
// ---------------------------------------------------------------------------

#ifndef LOOM_EVENT_H
#define LOOM_EVENT_H

#include <stdint.h>
#include <string.h>  // memcpy, memset

// ---------------------------------------------------------------------------
// Signal type
// ---------------------------------------------------------------------------
// Signals identify the *kind* of event. The framework reserves signals 0 and
// 1 for ENTRY/EXIT. User signals start at 2 (use SIGNAL_USER as a base).
// ---------------------------------------------------------------------------

using Signal = uint16_t;

static constexpr Signal SIGNAL_ENTRY = 0;
static constexpr Signal SIGNAL_EXIT  = 1;
static constexpr Signal SIGNAL_USER  = 2;  // first user-definable signal

// ---------------------------------------------------------------------------
// Payload size
// ---------------------------------------------------------------------------

#ifndef LOOM_EVENT_PAYLOAD_SIZE
#define LOOM_EVENT_PAYLOAD_SIZE  32
#endif

// ---------------------------------------------------------------------------
// Event struct
// ---------------------------------------------------------------------------

struct Event {
    Signal  signal;
    uint8_t payload[LOOM_EVENT_PAYLOAD_SIZE];

    // --- Typed payload accessors -----------------------------------------

    /// Store a value of type T into the payload.
    /// Fails at compile time if T is too large.
    template <typename T>
    void setPayload(T const& value) {
        static_assert(sizeof(T) <= LOOM_EVENT_PAYLOAD_SIZE, "Payload too large for Event");
        memcpy(payload, &value, sizeof(T));
    }

    /// Retrieve a value of type T from the payload.
    /// Fails at compile time if T is too large.
    template <typename T>
    T getPayload() const {
        static_assert(sizeof(T) <= LOOM_EVENT_PAYLOAD_SIZE, "Payload too large for Event");
        T value;
        memcpy(&value, payload, sizeof(T));
        return value;
    }

    // --- Convenience factory ---------------------------------------------

    /// Create an Event with only a signal (payload zeroed).
    static Event make(Signal sig) {
        Event e;
        e.signal = sig;
        memset(e.payload, 0, sizeof(e.payload));
        return e;
    }
};

#endif // LOOM_EVENT_H
