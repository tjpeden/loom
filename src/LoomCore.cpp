// ---------------------------------------------------------------------------
// Loom — LoomCore Implementation
// ---------------------------------------------------------------------------

#include "LoomCore.h"
#include "Actor.h"

// ---------------------------------------------------------------------------
// Global Loom instance
// ---------------------------------------------------------------------------

LoomCore Loom;

bool LoomCore::registerActor(ActorBase& actor, UBaseType_t priority) {
    if (_actorCount >= LOOM_MAX_ACTORS) {
        LOOM_WARN("Actor registry full (max %d)", LOOM_MAX_ACTORS);
        return false;
    }

    actor._priority = priority;
    actor._bus      = &_bus;

    _actors[_actorCount] = &actor;
    _actorCount++;

    LOOM_LOG("Registered actor=%s (priority=%u)", actor.name(), (unsigned)priority);
    return true;
}

bool LoomCore::subscribe(Signal signal, ActorBase& actor) {
    return _bus.subscribe(signal, actor);
}

void LoomCore::begin() {
    LOOM_LOG("Loom starting (%u actors registered)", (unsigned)_actorCount);

    // Suspend the scheduler so newly created tasks cannot run until all
    // actors have been fully initialized. xTaskCreateStatic creates tasks
    // in the Ready state — without this guard, a high-priority task could
    // preempt and call begin() before other actors' queues exist.
    vTaskSuspendAll();

    for (uint8_t i = 0; i < _actorCount; i++) {
        _actors[i]->_initResources();
    }

    // Resume the scheduler. All tasks become eligible to run simultaneously.
    // This guarantees every actor's queue and task exist before any actor's
    // begin() method executes.
    xTaskResumeAll();

    LOOM_LOG("Loom started. %u actors running.", (unsigned)_actorCount);
}

void LoomCore::publishEvent(Event const& event) {
    _bus.publishEvent(event);
}
