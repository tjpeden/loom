// ---------------------------------------------------------------------------
// Loom — Bus (Publish / Subscribe)
// ---------------------------------------------------------------------------
// The Bus is a static subscription table mapping Signals to ActorBase
// pointers. Subscriptions are registered during setup() and are immutable
// after Loom.begin(), so concurrent reads are safe without synchronization.
// ---------------------------------------------------------------------------

#ifndef LOOM_BUS_H
#define LOOM_BUS_H

#include "Platform.h"
#include "Event.h"

// Forward declaration — full definition is in Actor.h
class ActorBase;

// ---------------------------------------------------------------------------
// Subscription entry
// ---------------------------------------------------------------------------

struct Subscription {
    Signal     signal;
    ActorBase* actor;
};

// ---------------------------------------------------------------------------
// Bus class
// ---------------------------------------------------------------------------

class Bus {
public:
    /// Register a subscription: actor will receive events with the given signal.
    /// Must be called before Loom.begin(). Returns true on success, false if
    /// the subscription table is full.
    bool subscribe(Signal signal, ActorBase& actor);

    /// Publish an event to all actors subscribed to event.signal.
    /// Safe to call from any Actor task — the subscription table is read-only
    /// after Loom.begin().
    void publishEvent(Event const& event);

    /// Returns the current number of subscriptions.
    uint16_t subscriptionCount() const { return _count; }

private:
    Subscription _table[LOOM_MAX_SUBSCRIPTIONS];
    uint16_t     _count = 0;
};

#endif // LOOM_BUS_H
