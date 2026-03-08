// ---------------------------------------------------------------------------
// Loom — LoomCore (Actor Registry & Startup Orchestration)
// ---------------------------------------------------------------------------
// LoomCore manages the actor registry, bus subscriptions, and the begin()
// startup sequence. A single global instance is exposed as `Loom`.
// ---------------------------------------------------------------------------

#ifndef LOOM_LOOMCORE_H
#define LOOM_LOOMCORE_H

#include "Platform.h"
#include "Event.h"
#include "Bus.h"

// Forward declaration
class ActorBase;

class LoomCore {
public:
    /// Register an actor with a given priority. Must be called before begin().
    /// Returns true on success, false if the registry is full.
    bool registerActor(ActorBase& actor, UBaseType_t priority = 1);

    /// Subscribe an actor to a signal on the bus. Must be called before begin().
    bool subscribe(Signal signal, ActorBase& actor);

    /// Initialize all actors and start the framework.
    /// Creates FreeRTOS tasks (suspended), then resumes all together.
    void begin();

    /// Publish an event via the bus (convenience for non-actor code).
    void publishEvent(Event const& event);

    /// Returns the number of registered actors.
    uint8_t actorCount() const { return _actorCount; }

private:
    ActorBase* _actors[LOOM_MAX_ACTORS] = {};
    uint8_t    _actorCount = 0;
    Bus        _bus;
};

#endif // LOOM_LOOMCORE_H
