// ---------------------------------------------------------------------------
// Loom — Bus Implementation
// ---------------------------------------------------------------------------

#include "Bus.h"
#include "Actor.h"   // For ActorBase::postEvent

bool Bus::subscribe(Signal signal, ActorBase& actor) {
    if (_count >= LOOM_MAX_SUBSCRIPTIONS) {
        LOOM_WARN("Subscription table full (max %d)", LOOM_MAX_SUBSCRIPTIONS);
        return false;
    }
    _table[_count].signal = signal;
    _table[_count].actor  = &actor;
    _count++;

    LOOM_LOG("Subscribed actor=%s to signal=%u", actor.name(), (unsigned)signal);
    return true;
}

void Bus::publishEvent(Event const& event) {
    for (uint16_t i = 0; i < _count; i++) {
        if (_table[i].signal == event.signal) {
            _table[i].actor->postEvent(event);
        }
    }
}
