// ---------------------------------------------------------------------------
// Loom — Actor Base Class & CRTP Template
// ---------------------------------------------------------------------------
// ActorBase is the non-templated base that holds FreeRTOS internals common
// to all actors (task handle, timer slots, name, priority, bus pointer).
//
// Actor<QueueCapacity, Derived> is the CRTP template that adds the
// statically-sized queue storage and type-safe state handler pointers.
//
// All FreeRTOS resources (task stack, TCB, queue storage, queue control block,
// timer slots) are embedded directly in the Actor object — no dynamic
// allocation.
// ---------------------------------------------------------------------------

#ifndef LOOM_ACTOR_H
#define LOOM_ACTOR_H

#include "Platform.h"
#include "Event.h"

// Forward declaration — full definition is in Bus.h
class Bus;

// ---------------------------------------------------------------------------
// Timer handle — an index into the per-actor timer slot array
// ---------------------------------------------------------------------------

using TimerHandle = int8_t;
static constexpr TimerHandle LOOM_INVALID_TIMER = -1;

// ---------------------------------------------------------------------------
// ActorBase — non-templated base class
// ---------------------------------------------------------------------------
// Contains everything that doesn't depend on template parameters:
//   - FreeRTOS task handle
//   - Timer slot management
//   - Name and priority
//   - Pointer to the global Bus (set by LoomCore during registration)
//   - Virtual interface for begin(), queue access, and dispatch
// ---------------------------------------------------------------------------

class ActorBase {
public:
    explicit ActorBase(const char* actorName)
        : _name(actorName) {}

    // No copying or moving
    ActorBase(ActorBase const&) = delete;
    ActorBase& operator=(ActorBase const&) = delete;

    virtual ~ActorBase() = default;

    // --- Identity --------------------------------------------------------

    const char* name() const { return _name; }
    UBaseType_t priority() const { return _priority; }

    // --- Event posting (public API) --------------------------------------

    /// Post an event into this actor's queue. Returns true on success.
    bool postEvent(Event const& event) {
        QueueHandle_t q = queueHandle();
        if (q == nullptr) return false;

        BaseType_t ok = xQueueSend(q, &event, 0);
        if (ok != pdTRUE) {
            LOOM_WARN("Queue full, dropped signal=%u for actor=%s",
                      (unsigned)event.signal, _name);
            return false;
        }
        return true;
    }

    /// ISR-safe / BLE-callback-safe post. Returns true on success.
    bool postFromISR(Event const& event) {
        QueueHandle_t q = queueHandle();
        if (q == nullptr) return false;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        BaseType_t ok = xQueueSendFromISR(q, &event, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

        if (ok != pdTRUE) {
            // Note: no debug print from ISR (not safe)
            return false;
        }
        return true;
    }

    // --- Timer API -------------------------------------------------------

    /// Start a one-shot timer that posts `signal` to this actor after `ms`.
    TimerHandle startTimer(Signal signal, uint32_t ms);

    /// Start a repeating timer that posts `signal` every `ms`.
    TimerHandle startRepeatingTimer(Signal signal, uint32_t ms);

    /// Cancel a specific timer by handle.
    void cancelTimerByHandle(TimerHandle handle);

    /// Cancel ALL timers that post the given signal.
    void cancelTimer(Signal signal);

    /// Cancel all active timers for this actor.
    void cancelAllTimers();

    // --- Publishing (convenience — delegates to Bus) ---------------------

    void publishEvent(Event const& event);

protected:
    // --- Subclass interface ----------------------------------------------

    /// Called once from the actor's own task context during startup.
    /// The derived class should do hardware init and call transitionTo().
    virtual void begin() = 0;

    /// Returns the FreeRTOS queue handle (owned by the derived template).
    virtual QueueHandle_t queueHandle() = 0;

    /// Dispatch an event to the current state handler.
    virtual void dispatch(Event const& event) = 0;

    /// Returns true if the actor has a current state (transitionTo was called).
    virtual bool hasState() const = 0;

    /// Create FreeRTOS queue and task. Called by LoomCore while the
    /// scheduler is suspended — tasks cannot run until xTaskResumeAll().
    virtual void _initResources() = 0;

private:
    friend class LoomCore;  // LoomCore sets _priority, _bus, and drives startup

    // --- Timer internals -------------------------------------------------

    struct TimerSlot {
        StaticTimer_t  timerBuffer;
        TimerHandle_t  handle;   // FreeRTOS timer handle (NULL if unused)
        Signal         signal;
        ActorBase*     owner;
        bool           active;
        bool           repeating;
    };

    static void timerCallback(TimerHandle_t xTimer);

    TimerHandle findFreeTimerSlot();

    // --- Data members ----------------------------------------------------

    const char*   _name;
    UBaseType_t   _priority = 1;
    Bus*          _bus      = nullptr;

    TimerSlot     _timers[LOOM_MAX_TIMERS_PER_ACTOR] = {};

protected:
    // --- Task entry point ------------------------------------------------
    // This runs the actor's event loop: calls begin(), then blocks on queue.
    // Protected so the derived template's static trampoline can call it.
    void _taskEntry();
};

// ---------------------------------------------------------------------------
// Actor<QueueCapacity, Derived> — CRTP template
// ---------------------------------------------------------------------------
// Template parameters:
//   QueueCapacity — number of Event slots in this actor's queue
//   Derived       — the user's concrete class (for type-safe state pointers)
//
// Optional:
//   StackSize     — stack size in bytes (default LOOM_DEFAULT_STACK_SIZE)
// ---------------------------------------------------------------------------

template <uint16_t QueueCapacity, typename Derived, uint32_t StackSize = LOOM_DEFAULT_STACK_SIZE>
class Actor : public ActorBase {
public:
    /// State handler type — pointer to member function of the Derived class.
    using StateHandler = void (Derived::*)(Event const&);

    explicit Actor(const char* actorName)
        : ActorBase(actorName), _currentState(nullptr) {}

protected:
    // --- State machine ---------------------------------------------------

    /// Transition to a new state. Dispatches SIGNAL_EXIT to the current
    /// state (unless this is the initial transition from begin()) and
    /// SIGNAL_ENTRY to the new state.
    void transitionTo(StateHandler newState) {
        Derived* self = static_cast<Derived*>(this);

        if (_currentState != nullptr) {
            // Exit the current state
            Event exitEvent = Event::make(SIGNAL_EXIT);
            (self->*_currentState)(exitEvent);

            LOOM_LOG("%s: state transition (exit -> entry)", name());
        } else {
            LOOM_LOG("%s: initial transition (entry)", name());
        }

        _currentState = newState;

        // Enter the new state
        Event entryEvent = Event::make(SIGNAL_ENTRY);
        (self->*_currentState)(entryEvent);
    }

    // --- Queue handle (override) -----------------------------------------

    QueueHandle_t queueHandle() override {
        return _queueHandle;
    }

    // --- hasState (override) ---------------------------------------------

    bool hasState() const override {
        return _currentState != nullptr;
    }

    // --- Dispatch (override) ---------------------------------------------

    void dispatch(Event const& event) override {
        if (_currentState != nullptr) {
            Derived* self = static_cast<Derived*>(this);
            (self->*_currentState)(event);
        }
    }

private:
    friend class LoomCore;

    // --- State -----------------------------------------------------------

    StateHandler _currentState;

    // --- FreeRTOS task storage (static) -----------------------------------

    static constexpr uint32_t STACK_WORDS = loomStackWords(StackSize);

    StackType_t    _stackBuffer[STACK_WORDS];
    StaticTask_t   _taskTCB;
    TaskHandle_t   _taskHandle = nullptr;

    // --- FreeRTOS queue storage (static) ---------------------------------

    uint8_t        _queueStorage[QueueCapacity * sizeof(Event)];
    StaticQueue_t  _queueControlBlock;
    QueueHandle_t  _queueHandle = nullptr;

    // --- Initialization (called by LoomCore::begin via ActorBase) ---------
    // The scheduler is suspended when this runs, so the new task cannot
    // be scheduled until LoomCore calls xTaskResumeAll().

    void _initResources() override {
        // Create the queue
        _queueHandle = xQueueCreateStatic(
            QueueCapacity,
            sizeof(Event),
            _queueStorage,
            &_queueControlBlock
        );

        // Create the task. The scheduler is suspended by LoomCore, so this
        // task will not run until all actors have been initialized.
        _taskHandle = xTaskCreateStatic(
            _taskEntryTrampoline,
            name(),              // Task name (max 8 chars on nRF52)
            STACK_WORDS,
            this,                // pvParameters — pointer to this actor
            priority(),
            _stackBuffer,
            &_taskTCB
        );

        LOOM_LOG("Created task for actor=%s (priority=%u, stack=%lu)",
                 name(), (unsigned)priority(), (unsigned long)StackSize);
    }

    // Static trampoline for xTaskCreateStatic
    static void _taskEntryTrampoline(void* pvParameters) {
        ActorBase* actor = static_cast<ActorBase*>(pvParameters);
        actor->_taskEntry();
    }
};

#endif // LOOM_ACTOR_H
