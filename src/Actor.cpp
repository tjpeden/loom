// ---------------------------------------------------------------------------
// Loom — ActorBase Implementation
// ---------------------------------------------------------------------------
// Non-template methods: timer management, task entry loop, publishEvent.
// ---------------------------------------------------------------------------

#include "Actor.h"
#include "Bus.h"

// ---------------------------------------------------------------------------
// Task entry — the event loop
// ---------------------------------------------------------------------------

void ActorBase::_taskEntry() {
    // Phase 1: call user's begin() in this actor's task context
    begin();

    // Check that begin() established an initial state
    if (!hasState()) {
        LOOM_WARN("Actor=%s has no initial state after begin() — did you forget to call transitionTo()?", _name);
    }

    // Phase 2: event loop — blocks on queue, dispatches to current state
    QueueHandle_t q = queueHandle();

    for (;;) {
        Event event;
        // Block indefinitely until an event arrives
        if (xQueueReceive(q, &event, portMAX_DELAY) == pdTRUE) {

#ifdef LOOM_DEBUG_ENABLED
            uint32_t startMs = loomMillis();
#endif

            LOOM_LOG("Dispatch signal=%u to actor=%s",
                     (unsigned)event.signal, _name);

            dispatch(event);

#ifdef LOOM_DEBUG_ENABLED
            uint32_t elapsed = loomMillis() - startMs;
            if (elapsed > LOOM_SLOW_HANDLER_THRESHOLD_MS) {
                LOOM_WARN("Slow handler in actor=%s (%lums > %dms threshold)",
                          _name, (unsigned long)elapsed,
                          LOOM_SLOW_HANDLER_THRESHOLD_MS);
            }
#endif
        }
    }
}

// ---------------------------------------------------------------------------
// Publishing — delegates to Bus
// ---------------------------------------------------------------------------

void ActorBase::publishEvent(Event const& event) {
    if (_bus != nullptr) {
        _bus->publishEvent(event);
    }
}

// ---------------------------------------------------------------------------
// Timer callback — static, called in FreeRTOS timer daemon context
// ---------------------------------------------------------------------------

void ActorBase::timerCallback(TimerHandle_t xTimer) {
    // pvTimerGetTimerID returns the pointer we stored — a TimerSlot*
    TimerSlot* slot = static_cast<TimerSlot*>(pvTimerGetTimerID(xTimer));
    if (slot == nullptr || slot->owner == nullptr) return;

    // Post the timer signal as an event to the owning actor's queue
    Event event = Event::make(slot->signal);
    slot->owner->postEvent(event);

    LOOM_LOG("Timer fired signal=%u for actor=%s",
             (unsigned)slot->signal, slot->owner->name());

    // One-shot timers: FreeRTOS auto-stops after firing. Mark slot inactive
    // so it can be reused. Repeating timers stay active.
    if (!slot->repeating) {
        slot->active = false;
    }
}

// ---------------------------------------------------------------------------
// Timer slot management
// ---------------------------------------------------------------------------

TimerHandle ActorBase::findFreeTimerSlot() {
    for (int8_t i = 0; i < LOOM_MAX_TIMERS_PER_ACTOR; i++) {
        if (!_timers[i].active) {
            return i;
        }
    }
    return LOOM_INVALID_TIMER;
}

TimerHandle ActorBase::startTimer(Signal signal, uint32_t ms) {
    TimerHandle slot = findFreeTimerSlot();
    if (slot == LOOM_INVALID_TIMER) {
        LOOM_WARN("No free timer slot for actor=%s (max %d)",
                  _name, LOOM_MAX_TIMERS_PER_ACTOR);
        return LOOM_INVALID_TIMER;
    }

    // If this slot previously held a timer, stop it before overwriting
    // the StaticTimer_t buffer to avoid corrupting FreeRTOS internals.
    if (_timers[slot].handle != nullptr) {
        xTimerStop(_timers[slot].handle, 0);
        _timers[slot].handle = nullptr;
    }

    _timers[slot].signal    = signal;
    _timers[slot].owner     = this;
    _timers[slot].active    = true;
    _timers[slot].repeating = false;

    _timers[slot].handle = xTimerCreateStatic(
        "",                              // timer name (not used)
        loomMsToTicks(ms),               // period in ticks
        pdFALSE,                         // one-shot (auto-reload = false)
        &_timers[slot],                  // pvTimerID — pointer to our slot
        timerCallback,                   // callback
        &_timers[slot].timerBuffer       // static storage
    );

    xTimerStart(_timers[slot].handle, 0);

    LOOM_LOG("Started one-shot timer signal=%u (%lums) for actor=%s [slot %d]",
             (unsigned)signal, (unsigned long)ms, _name, slot);

    return slot;
}

TimerHandle ActorBase::startRepeatingTimer(Signal signal, uint32_t ms) {
    TimerHandle slot = findFreeTimerSlot();
    if (slot == LOOM_INVALID_TIMER) {
        LOOM_WARN("No free timer slot for actor=%s (max %d)",
                  _name, LOOM_MAX_TIMERS_PER_ACTOR);
        return LOOM_INVALID_TIMER;
    }

    // If this slot previously held a timer, stop it before overwriting
    // the StaticTimer_t buffer to avoid corrupting FreeRTOS internals.
    if (_timers[slot].handle != nullptr) {
        xTimerStop(_timers[slot].handle, 0);
        _timers[slot].handle = nullptr;
    }

    _timers[slot].signal    = signal;
    _timers[slot].owner     = this;
    _timers[slot].active    = true;
    _timers[slot].repeating = true;

    _timers[slot].handle = xTimerCreateStatic(
        "",                              // timer name
        loomMsToTicks(ms),               // period in ticks
        pdTRUE,                          // repeating (auto-reload = true)
        &_timers[slot],                  // pvTimerID
        timerCallback,                   // callback
        &_timers[slot].timerBuffer       // static storage
    );

    xTimerStart(_timers[slot].handle, 0);

    LOOM_LOG("Started repeating timer signal=%u (%lums) for actor=%s [slot %d]",
             (unsigned)signal, (unsigned long)ms, _name, slot);

    return slot;
}

void ActorBase::cancelTimerByHandle(TimerHandle handle) {
    if (handle < 0 || handle >= LOOM_MAX_TIMERS_PER_ACTOR) return;
    if (!_timers[handle].active) return;

    xTimerStop(_timers[handle].handle, 0);
    _timers[handle].active = false;

    LOOM_LOG("Cancelled timer [slot %d] for actor=%s", handle, _name);
}

void ActorBase::cancelTimer(Signal signal) {
    for (int8_t i = 0; i < LOOM_MAX_TIMERS_PER_ACTOR; i++) {
        if (_timers[i].active && _timers[i].signal == signal) {
            xTimerStop(_timers[i].handle, 0);
            _timers[i].active = false;

            LOOM_LOG("Cancelled timer signal=%u [slot %d] for actor=%s",
                     (unsigned)signal, i, _name);
        }
    }
}

void ActorBase::cancelAllTimers() {
    for (int8_t i = 0; i < LOOM_MAX_TIMERS_PER_ACTOR; i++) {
        if (_timers[i].active) {
            xTimerStop(_timers[i].handle, 0);
            _timers[i].active = false;
        }
    }

    LOOM_LOG("Cancelled all timers for actor=%s", _name);
}
