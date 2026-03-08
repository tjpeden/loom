# Loom — Real-Time Embedded Framework for Arduino

**Version:** 0.2.1 (Design Draft)
**Date:** 2026-03-06
**Status:** Pre-Implementation Design Document

---

## Table of Contents

1. [Vision & Goals](#1-vision--goals)
2. [Inspiration & Differentiation](#2-inspiration--differentiation)
3. [Target Platform](#3-target-platform)
4. [Core Concepts](#4-core-concepts)
   - 4.1 [Actors](#41-actors)
   - 4.2 [Events](#42-events)
   - 4.3 [The Event Queue](#43-the-event-queue)
   - 4.4 [State Machines](#44-state-machines)
   - 4.5 [Scheduling](#45-scheduling)
   - 4.6 [Time Events](#46-time-events)
   - 4.7 [The Bus (Publish / Subscribe)](#47-the-bus-publish--subscribe)
5. [Architecture Overview](#5-architecture-overview)
6. [Design Constraints & Principles](#6-design-constraints--principles)
7. [API Design](#7-api-design)
   - 7.1 [Defining Events & Signals](#71-defining-events--signals)
   - 7.2 [Defining an Actor](#72-defining-an-actor)
   - 7.3 [Defining a State Machine](#73-defining-a-state-machine)
   - 7.4 [Time Events](#74-time-events)
   - 7.5 [Posting Events](#75-posting-events)
   - 7.6 [Publishing Events](#76-publishing-events)
   - 7.7 [Wiring Everything Up (sketch)](#77-wiring-everything-up-sketch)
8. [Full Example: Traffic Light Controller](#8-full-example-traffic-light-controller)
9. [Memory Model](#9-memory-model)
10. [Actor Lifecycle & RTOS Integration](#10-actor-lifecycle--rtos-integration)
11. [Interrupt & BLE Callback Safety](#11-interrupt--ble-callback-safety)
12. [Debugging](#12-debugging)
13. [File & Library Structure](#13-file--library-structure)
14. [Constraints, Limitations & Non-Goals](#14-constraints-limitations--non-goals)
15. [Glossary](#15-glossary)
16. [Open Questions](#16-open-questions)

---

## 1. Vision & Goals

**Loom** is an event-driven, real-time embedded framework for Arduino. It brings
the Actor model and flat finite state machines to hobbyists and makers — backed
by FreeRTOS for true preemptive multitasking, without requiring the user to
learn RTOS concepts directly.

### Primary Goals

| Goal | Description |
|---|---|
| **Hobbyist-Friendly** | A beginner should understand their first Actor within 10 minutes of reading an example. |
| **No Dynamic Allocation** | All memory is statically allocated at compile time. Actor objects embed their own FreeRTOS stack buffers, queue storage, and control blocks. The compiler's memory report reflects actual usage. |
| **No Blocking** | Actors never call `delay()`. Time is managed by the framework via Time Events. |
| **Readable Code** | No cryptic macros or abbreviations. Names are long if they need to be. |
| **Predictable Resource Usage** | Memory and CPU usage are deterministic. Stack sizes, queue depths, and timer counts are configured at compile time with sensible defaults. |
| **Arduino-Native Feel** | Fits naturally into `setup()`. No external tooling required. |
| **True Concurrency** | Each Actor runs in its own FreeRTOS task with preemptive priority scheduling. High-priority Actors respond immediately, even if a low-priority Actor is mid-handler. |

---

## 2. Inspiration & Differentiation

Loom draws conceptual inspiration from **QP by Quantum Leaps** — the gold
standard of event-driven embedded frameworks. However, QP carries significant
complexity: UML statecharts, a separate modeling tool (QM), hierarchical state
machines, and a steep learning curve.

Loom trades QP's power for accessibility:

| Feature | QP / QP-Arduino | Loom |
|---|---|---|
| State machine depth | Hierarchical (UML) | Flat (simple, explicit transitions) |
| Tooling required | QM modeling tool | None — pure code |
| RTOS integration | Optional, complex setup | Built-in, hidden from the user |
| Dynamic allocation | Optional | Prohibited (fully static) |
| Learning curve | Steep | Gentle |
| Publish / Subscribe | Yes | Yes |
| Time events | Yes (1 per active object) | Yes (multiple per Actor) |
| Target user | Professional / Advanced | Hobbyist → Intermediate |

Loom is **not** trying to replace QP. It is a gentler on-ramp to event-driven
embedded thinking, with real preemptive scheduling under the hood.

---

## 3. Target Platform

Loom targets the **nRF52840** as its baseline platform, specifically the
**Adafruit CLUE** board running Adafruit's nRF52 Arduino core (Bluefruit).

| Spec | Value |
|---|---|
| MCU | Nordic nRF52840 |
| Architecture | ARM Cortex-M4F |
| Clock | 64 MHz |
| RAM | 256 KB |
| Flash | 1 MB |
| FPU | Yes (hardware single-precision) |
| BLE | Yes (Nordic SoftDevice) |
| RTOS | FreeRTOS (bundled with Adafruit BSP) |

### Adafruit CLUE Peripherals

The CLUE board provides a rich set of onboard peripherals that are natural
candidates for Actor decomposition:

| Peripheral | Description |
|---|---|
| TFT Display | 240x240 1.3" IPS (ST7789) |
| Accelerometer + Gyro | LSM6DS33 (6-axis IMU) |
| Magnetometer | LIS3MDL (3-axis) |
| Barometer + Temperature | BMP280 |
| Humidity + Temperature | SHT30 |
| Proximity / Light / Color | APDS9960 |
| Microphone | PDM MEMS |
| Speaker | Buzzer |
| NeoPixel | 1x addressable RGB LED |
| Buttons | 2x user buttons (A and B) |
| GPIO | Micro:bit-compatible edge connector |

> **Design rule:** All design decisions are made for the nRF52840 / Cortex-M4F
> with 256 KB of RAM. Support for other boards may be added later through the
> Platform abstraction layer, but the nRF52840 is the only board that must work.

---

## 4. Core Concepts

### 4.1 Actors

An **Actor** is an autonomous, self-contained unit of behavior. It owns:

- A **FreeRTOS task** (its execution context, with statically allocated stack)
- A **FreeRTOS queue** (its event inbox, with statically allocated storage)
- A **current state** (a pointer-to-member-function of the derived class)
- Its own **private data** (no shared mutable state between Actors)

Actors communicate **exclusively** through events. They never call each other's
methods directly. This eliminates an entire class of concurrency bugs — each
Actor's data is only ever accessed from its own task, so no mutexes are needed
within an Actor.

Each Actor processes one event at a time (run-to-completion within that Actor).
However, because each Actor is a separate FreeRTOS task, multiple Actors can be
ready to run simultaneously, and the RTOS scheduler preempts lower-priority
Actors in favor of higher-priority ones.

The Actor base class uses the **Curiously Recurring Template Pattern (CRTP)**
so that state handler function pointers are stored with the correct derived
type, without type-erasure overhead:

```cpp
class MyActor : public Actor<8, MyActor> { ... };
//                           ^  ^^^^^^^
//                     queue depth  derived type (CRTP)
```

```
┌─────────────────────────────────────┐
│             Actor                   │
│                                     │
│  ┌──────────────┐   ┌────────────┐  │
│  │   FreeRTOS   │   │  Current   │  │
│  │    Queue     │──▶│   State    │  │
│  │  (static    │   │ (member    │  │
│  │   storage)  │   │  fn ptr)   │  │
│  └──────────────┘   └────────────┘  │
│                                     │
│  [ private data fields ]            │
│                                     │
│  ┌──────────────┐                   │
│  │  FreeRTOS    │                   │
│  │  Task        │                   │
│  │ (static     │                   │
│  │  stack buf) │                   │
│  └──────────────┘                   │
└─────────────────────────────────────┘
```

### 4.2 Events

An **Event** is a small, immutable message. Every event has a **signal** — a
numeric identifier (`uint16_t`) that tells the receiving Actor what happened.
Events carry a fixed-size payload for data.

Events are value types. They are copied into the queue, not heap-allocated.
This keeps things safe, simple, and deterministic.

```
┌───────────────────────────────────┐
│            Event                  │
│  signal  : uint16_t              │  ← what happened (up to 65,535 signals)
│  payload : uint8_t[32]           │  ← 32 bytes of data
└───────────────────────────────────┘
```

The 32-byte payload comfortably fits common embedded data. The framework
provides accessor methods for common primitive types, and users can define
their own structs and cast or `memcpy` into the payload for structured data:

| Use Case | Size |
|---|---|
| A boolean flag | 1 byte |
| A pin number + value | 2 bytes |
| A 3-axis accelerometer reading (3 floats) | 12 bytes |
| A full 6-axis IMU reading (6 floats) | 24 bytes |
| 6 floats + a `uint32_t` timestamp | 28 bytes |

### 4.3 The Event Queue

Each Actor's event queue is a **statically allocated FreeRTOS queue**
(`xQueueCreateStatic`). The queue depth is set at compile time via a template
parameter on the Actor class. The queue storage buffer is embedded directly in
the Actor object.

When the queue is full, a post attempt returns a failure status and a debug
warning is emitted (if debug mode is enabled). Choosing queue depth is a tuning
concern — too small and you drop events under load; too large and the Actor
object consumes more memory (though with 256 KB this is rarely a constraint).

An Actor's task blocks on its queue (`xQueueReceive`) when no events are
pending. This means idle Actors consume zero CPU — the RTOS only schedules
them when work arrives.

### 4.4 State Machines

Each Actor implements a **flat finite state machine**. A state is a plain C++
member function with the signature `void(Event const&)`. The Actor holds a
pointer-to-member-function pointing to its current state handler.

When an event arrives, the framework calls the current state function with that
event. The state function may:

- Handle the event and stay in the same state
- Handle the event and **transition** to a new state (calling `transitionTo()`)
- **Ignore** the event (return without transitioning)

On a transition:
1. The current state receives a synthetic `SIGNAL_EXIT` event (for cleanup)
2. The next state receives a synthetic `SIGNAL_ENTRY` event (for initialization)
3. The Actor's state pointer is updated

#### Initial Transition

The first call to `transitionTo()` — made from within `begin()` — is the
**initial transition**. Because there is no prior state, the framework skips
the `SIGNAL_EXIT` dispatch and only sends `SIGNAL_ENTRY` to the target state.
Internally, the current state pointer is `nullptr` until the first
`transitionTo()` call; the framework checks for this to distinguish the initial
transition from subsequent ones.

### 4.5 Scheduling

Loom delegates all scheduling to **FreeRTOS**. There is no Loom-specific
scheduler.

Each Actor is a FreeRTOS task with a configurable priority. The RTOS scheduler
is preemptive: when a high-priority Actor receives an event (via a queue post),
it is immediately woken and preempts any lower-priority Actor that is currently
running.

The user assigns priorities when registering Actors. Loom provides sensible
defaults so beginners don't need to think about it.

### 4.6 Time Events

Time Events let an Actor schedule future events to be posted to itself — a
safe replacement for `delay()`. They are implemented using **FreeRTOS software
timers** (`xTimerCreateStatic`), which are managed by the RTOS timer daemon
task and do not require polling.

Time Events can be:
- **One-shot**: fires once after a delay
- **Repeating**: fires on a fixed interval until cancelled

Each Actor can have **multiple active timers** simultaneously. Timers are
identified by the signal they post, or by a handle returned from `startTimer()`
for cases where the same signal is used with different intervals.

#### Timer Callback Context

FreeRTOS software timer callbacks execute in the **timer daemon task**, not in
the owning Actor's task. When a timer fires, the callback posts the timer's
signal as an event to the owning Actor's queue using `xQueueSend()`. The Actor
then processes the event in its own task context, maintaining the
run-to-completion guarantee. This is transparent to the user — from the Actor's
perspective, a timer event arrives in the queue like any other event.

### 4.7 The Bus (Publish / Subscribe)

The **Bus** is a global publish/subscribe channel. Any Actor can publish an
event to the Bus. Any Actor that has subscribed to that event's signal will
receive a copy posted to its own queue.

Subscriptions are registered at startup (before `Loom.begin()`) and stored in
a static table. The subscription table is **read-only after `Loom.begin()`**,
so concurrent reads during event publishing are safe without synchronization.
The Bus supports a configurable maximum number of subscriptions (default: 64
entries), overridable with `LOOM_MAX_SUBSCRIPTIONS`.

When an Actor publishes an event, the Bus iterates the subscription table and
calls `xQueueSend()` on each matching subscriber's queue. Because the
subscription table is immutable at this point, this operation is safe even if
the publishing Actor is preempted mid-iteration by a higher-priority Actor that
also publishes.

---

## 5. Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                     User Sketch                          │
│  setup() { Loom.begin(); /* register actors */ }         │
│  loop()  { /* empty — Actors run in their own tasks */ } │
└───────────────────────────┬──────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────┐
│                        Loom Core                         │
│                                                          │
│   ┌────────────┐   ┌────────────┐   ┌─────────────────┐  │
│   │    Bus     │   │   Actor    │   │     Debug       │  │
│   │ (publish / │   │  Registry  │   │    (optional    │  │
│   │  subscribe)│   │            │   │    Serial)      │  │
│   └────────────┘   └────────────┘   └─────────────────┘  │
└──────────────────────────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────┐
│                      Actor Tasks                         │
│                                                          │
│   ┌───────────┐   ┌───────────┐   ┌───────────┐         │
│   │  Actor A  │   │  Actor B  │   │  Actor C  │  ...    │
│   │ task pri:1│   │ task pri:2│   │ task pri:1│         │
│   │ queue [8] │   │ queue [16]│   │ queue [8] │         │
│   │ state: s1 │   │ state: s2 │   │ state: s1 │         │
│   └───────────┘   └───────────┘   └───────────┘         │
└──────────────────────────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────┐
│                       FreeRTOS                           │
│       (preemptive scheduler, queues, software timers)    │
└──────────────────────────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────┐
│                  Platform Abstraction                    │
│         (hardware GPIO, BLE callbacks, Serial)           │
└──────────────────────────────────────────────────────────┘
```

---

## 6. Design Constraints & Principles

### No Dynamic Allocation

All memory is statically allocated at compile time. Actor objects embed their
FreeRTOS task stack buffers, queue storage, task control blocks, and queue
control blocks directly. The framework uses `xTaskCreateStatic`,
`xQueueCreateStatic`, and `xTimerCreateStatic` exclusively. No calls to
`malloc`, `new`, `pvPortMalloc`, or `free` — ever. The compiler's memory
report accurately reflects the system's actual RAM usage.

### No Blocking

State handler functions must never block. No `delay()`, no `while` loops
polling a pin, no `Serial.readString()`. If you need to wait for something,
start a Time Event and handle it when it fires. This rule is essential because
a blocked state handler holds up its Actor's entire event processing.

### Run-to-Completion Semantics

Each event is fully processed by an Actor before the next event is dequeued
from that Actor's queue. Within a single Actor, event handling is always
sequential — no concurrent access to the Actor's private data.

However, different Actors run in different FreeRTOS tasks and can preempt each
other. This is safe because Actors never share mutable state — they communicate
only through events.

### No Abbreviations in Public APIs

The public API uses full, descriptive names. `transitionTo` not `tran`.
`publishEvent` not `pub`. This makes code readable to beginners.

### Interrupts and BLE Callbacks are Sources, Not Handlers

Hardware interrupts (ISRs) and BLE callbacks should only post events into an
Actor's queue and return immediately. Business logic lives in state handlers,
not ISRs. Loom provides ISR-safe and callback-safe posting methods for this
purpose.

### Prefer Flat State Machines

Flat state machines are explicitly preferred over hierarchical ones for
simplicity. Actors that become too complex should be decomposed into multiple
cooperating Actors rather than adding state hierarchy.

---

## 7. API Design

### 7.1 Defining Events & Signals

Signals are defined as a `uint16_t` enum. The framework reserves signals 0
and 1 for `SIGNAL_ENTRY` and `SIGNAL_EXIT`. User-defined signals start at 2.

The `Event` struct contains a signal and a 32-byte payload. The payload is a
raw byte array; the framework provides typed accessor methods for common
primitives, and users define their own structs for structured data.

```cpp
// signals.h

enum Signal : uint16_t {
    // --- Reserved framework signals (do not reuse) ---
    SIGNAL_ENTRY = 0,
    SIGNAL_EXIT  = 1,

    // --- User-defined signals start here ---
    SIGNAL_BUTTON_PRESSED,
    SIGNAL_TIMEOUT,
    SIGNAL_SENSOR_READING,
    SIGNAL_IMU_DATA,
    // ...
};
```

```cpp
// Event (defined by the framework in Event.h)

struct Event {
    Signal  signal;
    uint8_t payload[32];

    // --- Typed payload accessors ---
    template <typename T>
    void setPayload(T const& value) {
        static_assert(sizeof(T) <= sizeof(payload), "Payload too large");
        memcpy(payload, &value, sizeof(T));
    }

    template <typename T>
    T getPayload() const {
        static_assert(sizeof(T) <= sizeof(payload), "Payload too large");
        T value;
        memcpy(&value, payload, sizeof(T));
        return value;
    }
};
```

Example usage with user-defined structs:

```cpp
// User defines their own payload structs
struct ImuReading {
    float accel[3];
    float gyro[3];
    uint32_t timestamp;
};
static_assert(sizeof(ImuReading) <= 32, "ImuReading exceeds payload size");

// Setting payload
Event event;
event.signal = SIGNAL_IMU_DATA;
event.setPayload(ImuReading{
    {0.1f, 0.2f, 9.8f},
    {0.01f, -0.02f, 0.0f},
    millis()
});

// Reading payload
auto reading = event.getPayload<ImuReading>();
float ax = reading.accel[0];
```

### 7.2 Defining an Actor

An Actor is a class that inherits from `Actor<QueueCapacity, DerivedClass>`
using CRTP. The first template parameter sets the queue depth. The second is
the derived class itself, enabling type-safe state handler function pointers.

```cpp
// ButtonActor.h
#include <Loom.h>

class ButtonActor : public Actor<8, ButtonActor> {
public:
    explicit ButtonActor(uint8_t pin)
        : Actor("Button"), _pin(pin) {}

    void begin() override {
        pinMode(_pin, INPUT_PULLUP);
        transitionTo(&ButtonActor::stateIdle);
    }

private:
    uint8_t _pin;

    // --- State Handlers ---
    void stateIdle(Event const& event);
    void stateDebouncing(Event const& event);
};
```

The string `"Button"` passed to the `Actor` constructor is the Actor's name,
used in debug output. On ARM, string literals are stored in flash automatically
— no special handling is needed.

### 7.3 Defining a State Machine

State handlers are member functions of the derived class with the signature
`void(Event const&)`. They receive the current event and call `transitionTo()`
to change state.

```cpp
// ButtonActor.cpp
#include "ButtonActor.h"

void ButtonActor::stateIdle(Event const& event) {
    switch (event.signal) {
        case SIGNAL_ENTRY:
            // Start polling the pin every 20ms
            startRepeatingTimer(SIGNAL_POLL_PIN, 20);
            break;

        case SIGNAL_POLL_PIN:
            if (digitalRead(_pin) == LOW) {
                transitionTo(&ButtonActor::stateDebouncing);
            }
            break;

        case SIGNAL_EXIT:
            cancelAllTimers();
            break;

        default:
            break;
    }
}

void ButtonActor::stateDebouncing(Event const& event) {
    switch (event.signal) {
        case SIGNAL_ENTRY:
            // Start a 50ms one-shot debounce timer
            startTimer(SIGNAL_TIMEOUT, 50);
            break;

        case SIGNAL_TIMEOUT:
            if (digitalRead(_pin) == LOW) {
                // Button confirmed pressed — tell the world
                Event pressedEvent;
                pressedEvent.signal = SIGNAL_BUTTON_PRESSED;
                publishEvent(pressedEvent);
            }
            transitionTo(&ButtonActor::stateIdle);
            break;

        case SIGNAL_EXIT:
            cancelAllTimers();
            break;

        default:
            break;
    }
}
```

### 7.4 Time Events

```cpp
// Start a one-shot timer (fires once after `milliseconds`)
TimerHandle startTimer(Signal signal, uint32_t milliseconds);

// Start a repeating timer (fires every `milliseconds`)
TimerHandle startRepeatingTimer(Signal signal, uint32_t milliseconds);

// Cancel a specific timer by handle
void cancelTimer(TimerHandle handle);

// Cancel a specific timer by signal (cancels ALL timers with that signal)
void cancelTimer(Signal signal);

// Cancel all active timers for this Actor
void cancelAllTimers();
```

Each Actor can have multiple active timers simultaneously. Timers are
statically allocated using `xTimerCreateStatic` and managed by the FreeRTOS
timer daemon task. The maximum number of concurrent timers per Actor is set
by a compile-time constant (default: `LOOM_MAX_TIMERS_PER_ACTOR = 4`).

When `cancelTimer(Signal)` is called, it cancels **all** active timers that
post that signal, resolving any ambiguity.

### 7.5 Posting Events

Post an event directly into another Actor's queue:

```cpp
// From anywhere — direct method call on the Actor
targetActor.postEvent(event);

// ISR-safe / BLE-callback-safe post (uses xQueueSendFromISR)
targetActor.postFromISR(event);
```

`postEvent()` returns `true` if the event was successfully enqueued, `false` if
the queue was full (and a debug warning is emitted). `postFromISR()` similarly
returns a boolean status and handles the FreeRTOS yield notification internally.

### 7.6 Publishing Events

Publish an event to all subscribers via the Bus:

```cpp
// From inside a state handler
publishEvent(event);

// From outside an Actor (e.g., a helper function)
Loom.publishEvent(event);
```

Subscribe an Actor to a signal during `setup()`, before `Loom.begin()`:

```cpp
Loom.subscribe(SIGNAL_BUTTON_PRESSED, ledActor);
Loom.subscribe(SIGNAL_BUTTON_PRESSED, buzzerActor);
```

### 7.7 Wiring Everything Up (sketch)

```cpp
#include <Loom.h>
#include "ButtonActor.h"
#include "LedActor.h"

ButtonActor buttonActor(2);  // pin 2
LedActor    ledActor(13);    // pin 13

void setup() {
    Serial.begin(115200);

    Loom.registerActor(buttonActor, /*priority=*/1);
    Loom.registerActor(ledActor,    /*priority=*/2);

    Loom.subscribe(SIGNAL_BUTTON_PRESSED, ledActor);

    Loom.begin();  // creates tasks (suspended), then resumes all
}

void loop() {
    // Empty — all work happens in Actor tasks
}
```

#### Startup Sequence

1. User's `setup()` runs: Serial init, Actor registration, Bus subscriptions
2. `Loom.begin()` is called:
   a. For each registered Actor, a FreeRTOS task is created in **suspended**
      state using `xTaskCreateStatic`
   b. Once all tasks are created, Loom **resumes all tasks** together
   c. Each Actor's task enters the framework event loop, which first calls
      the Actor's `begin()` method (within its own task context), then blocks
      on its queue waiting for events
3. User's `loop()` runs (typically empty)

The suspended-then-resume-all approach guarantees that all Actor queues exist
before any Actor's `begin()` method runs, preventing startup ordering bugs.

---

## 8. Full Example: Traffic Light Controller

This example demonstrates two cooperating Actors: a `TrafficLightActor` that
controls the lights and a `PedestrianButtonActor` that listens for button
presses and signals the light to change.

```cpp
// signals.h
enum Signal : uint16_t {
    SIGNAL_ENTRY = 0,
    SIGNAL_EXIT  = 1,

    SIGNAL_TIMEOUT,
    SIGNAL_PEDESTRIAN_REQUEST,
    SIGNAL_POLL_PIN,
};
```

```cpp
// PedestrianButtonActor.h
#include <Loom.h>
#include "signals.h"

class PedestrianButtonActor : public Actor<8, PedestrianButtonActor> {
public:
    explicit PedestrianButtonActor(uint8_t buttonPin)
        : Actor("PedestrianButton"), _buttonPin(buttonPin) {}

    void begin() override {
        pinMode(_buttonPin, INPUT_PULLUP);
        startRepeatingTimer(SIGNAL_POLL_PIN, 50);
        transitionTo(&PedestrianButtonActor::stateWaiting);
    }

private:
    uint8_t _buttonPin;

    void stateWaiting(Event const& event) {
        switch (event.signal) {
            case SIGNAL_POLL_PIN: {
                if (digitalRead(_buttonPin) == LOW) {
                    Event request;
                    request.signal = SIGNAL_PEDESTRIAN_REQUEST;
                    publishEvent(request);
                }
                break;
            }
            default:
                break;
        }
    }
};
```

```cpp
// TrafficLightActor.h
#include <Loom.h>
#include "signals.h"

class TrafficLightActor : public Actor<8, TrafficLightActor> {
public:
    TrafficLightActor(uint8_t redPin, uint8_t yellowPin, uint8_t greenPin)
        : Actor("TrafficLight"),
          _redPin(redPin), _yellowPin(yellowPin), _greenPin(greenPin) {}

    void begin() override {
        pinMode(_redPin,    OUTPUT);
        pinMode(_yellowPin, OUTPUT);
        pinMode(_greenPin,  OUTPUT);
        transitionTo(&TrafficLightActor::stateGreen);
    }

private:
    uint8_t _redPin;
    uint8_t _yellowPin;
    uint8_t _greenPin;
    bool    _pedestrianWaiting = false;

    void setLights(bool red, bool yellow, bool green) {
        digitalWrite(_redPin,    red    ? HIGH : LOW);
        digitalWrite(_yellowPin, yellow ? HIGH : LOW);
        digitalWrite(_greenPin,  green  ? HIGH : LOW);
    }

    void stateGreen(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                setLights(false, false, true);
                startTimer(SIGNAL_TIMEOUT, 10000);  // 10 seconds
                break;

            case SIGNAL_PEDESTRIAN_REQUEST:
                _pedestrianWaiting = true;
                break;

            case SIGNAL_TIMEOUT:
                transitionTo(&TrafficLightActor::stateYellow);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIGNAL_TIMEOUT);
                break;

            default:
                break;
        }
    }

    void stateYellow(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                setLights(false, true, false);
                startTimer(SIGNAL_TIMEOUT, 3000);  // 3 seconds
                break;

            case SIGNAL_TIMEOUT:
                transitionTo(&TrafficLightActor::stateRed);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIGNAL_TIMEOUT);
                break;

            default:
                break;
        }
    }

    void stateRed(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                setLights(true, false, false);
                _pedestrianWaiting = false;
                startTimer(SIGNAL_TIMEOUT, 8000);  // 8 seconds
                break;

            case SIGNAL_TIMEOUT:
                transitionTo(&TrafficLightActor::stateGreen);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIGNAL_TIMEOUT);
                break;

            default:
                break;
        }
    }
};
```

```cpp
// TrafficLight.ino
#include <Loom.h>
#include "PedestrianButtonActor.h"
#include "TrafficLightActor.h"

PedestrianButtonActor buttonActor(2);
TrafficLightActor     lightActor(9, 10, 11);

void setup() {
    Serial.begin(115200);

    Loom.registerActor(buttonActor, /*priority=*/1);
    Loom.registerActor(lightActor,  /*priority=*/2);

    Loom.subscribe(SIGNAL_PEDESTRIAN_REQUEST, lightActor);

    Loom.begin();
}

void loop() {
    // Empty — all work happens in Actor tasks
}
```

Notice: no `delay()`, no `millis()` management in user code, no shared global
state between the two Actors, and no `Loom.run()` — each Actor runs
independently in its own FreeRTOS task.

---

## 9. Memory Model

### Per-Actor Memory

All buffers are embedded directly in the Actor object (declared as a global
variable), so the compiler's memory report reflects actual usage.

| Resource | Size | Notes |
|---|---|---|
| FreeRTOS task stack buffer | 512 bytes (default) | Embedded in Actor object. Configurable. |
| FreeRTOS StaticTask_t (TCB) | ~168 bytes | Embedded in Actor object. |
| FreeRTOS queue storage | `N × sizeof(Event)` | ~34 bytes per slot (2B signal + 32B payload) |
| FreeRTOS StaticQueue_t | ~80 bytes | Embedded in Actor object. |
| Timer slots | `M × ~52 bytes` | StaticTimer_t per slot (default M=4) |
| Current state pointer | 8 bytes | ARM pointer-to-member-function |
| Actor name pointer | 4 bytes | Points to string literal in flash |
| Priority | 1 byte | |
| **Example total (N=8 queue, M=4 timers, 512B stack)** | **~1.3 KB** | |

### Framework Overhead

| Component | Size |
|---|---|
| Actor registry (up to 16 actors default) | 64 bytes (pointers) |
| Subscription table (64 entries default) | ~384 bytes |
| FreeRTOS timer daemon task | ~256 bytes stack (configurable) |
| FreeRTOS idle task | ~128 bytes stack |
| FreeRTOS kernel structures | ~500 bytes |
| **Total framework overhead** | **~1.3 KB** |

### Budget Perspective

With 256 KB of RAM, a system with 10 Actors (each with 8-slot queues, 4 timer
slots, and 512B stacks) uses approximately:

```
Framework overhead:     ~1.3 KB
10 Actors × ~1.3 KB:   ~13.0 KB
                        ─────────
Total:                  ~14.3 KB   (5.5% of available RAM)
```

This leaves over 240 KB free for sensor buffers, display framebuffers, BLE
stacks, and user data.

### Static Allocation Strategy

All Actor instances are declared as global variables in the sketch, placing
them in the `.bss` / `.data` segments. FreeRTOS tasks, queues, and timers are
created during `Loom.begin()` using the static allocation APIs
(`xTaskCreateStatic`, `xQueueCreateStatic`, `xTimerCreateStatic`) with buffers
embedded in the Actor objects. This means:

- The Arduino IDE's "Global variables use X bytes" message is accurate
- No runtime allocation failures are possible
- Memory usage is fully determined at compile time

---

## 10. Actor Lifecycle & RTOS Integration

### Task Function

Each Actor's FreeRTOS task runs a framework-internal loop:

```
ActorTaskFunction(actor):
    call actor.begin()              ← user-defined setup (set initial state, start timers)
    loop forever:
        event = xQueueReceive(actor.queue, portMAX_DELAY)   ← blocks until event
        call actor.currentState(event)                       ← run-to-completion
```

The task blocks on `xQueueReceive` when the queue is empty. The Actor consumes
zero CPU while idle.

### Priority Mapping

Loom priorities map directly to FreeRTOS task priorities. Higher numbers mean
higher priority.

```cpp
Loom.registerActor(sensorActor,  /*priority=*/1);   // low priority
Loom.registerActor(displayActor, /*priority=*/1);    // same priority
Loom.registerActor(motorActor,   /*priority=*/3);    // high priority
```

When a high-priority Actor's queue receives an event, the RTOS immediately
preempts any running lower-priority Actor and switches to the high-priority
Actor's task.

### Startup Sequence

1. User's `setup()` runs: Serial init, Actor registration, Bus subscriptions
2. `Loom.begin()` is called:
   a. For each registered Actor, a FreeRTOS task is created in **suspended**
      state using `xTaskCreateStatic`
   b. FreeRTOS queues and timer slots are initialized using the static storage
      embedded in each Actor object
   c. Once all tasks are created, Loom **resumes all tasks** simultaneously
      via `vTaskResume()`
   d. Each Actor's task calls `begin()`, then enters its event loop
3. User's `loop()` runs (typically empty)

---

## 11. Interrupt & BLE Callback Safety

### Hardware Interrupts

Hardware ISRs must return quickly. Loom provides `postFromISR()` as a method
on the Actor class, which uses FreeRTOS's `xQueueSendFromISR()` to safely post
events from interrupt context. This function handles the required
`portYIELD_FROM_ISR` notification to the scheduler.

```cpp
// In an ISR:
void encoderISR() {
    Event tickEvent;
    tickEvent.signal = SIGNAL_ENCODER_TICK;
    tickEvent.setPayload<uint8_t>(digitalRead(ENCODER_PIN_A));
    motorActor.postFromISR(tickEvent);
}
```

### BLE Callbacks

Adafruit's BLE callbacks run in the SoftDevice context and behave similarly to
ISRs. The same `postFromISR()` method is safe to use from BLE callbacks:

```cpp
// In a BLE characteristic write callback:
void onBLEWrite(uint16_t connHandle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    Event bleEvent;
    bleEvent.signal = SIGNAL_BLE_DATA_RECEIVED;
    memcpy(bleEvent.payload, data, min(len, sizeof(bleEvent.payload)));
    bleActor.postFromISR(bleEvent);
}
```

This pattern keeps BLE handling simple: the callback is a thin bridge that
converts BLE data into an event, and the Actor's state machine handles the
business logic.

---

## 12. Debugging

### Serial Debug Output

When compiled with `LOOM_DEBUG_ENABLED` defined, Loom emits structured debug
output over Serial. This includes:

- Event dispatches: `[Loom] Dispatch signal=3 to actor=TrafficLight`
- State transitions: `[Loom] TrafficLight: stateGreen -> stateYellow`
- Queue overflows: `[Loom] WARNING: Queue full, dropped signal=5 for actor=Led`
- Timer fires: `[Loom] Timer fired signal=1 for actor=Button`
- Task info: `[Loom] Created task for actor=Button (priority=1, stack=512)`

In release builds (`LOOM_DEBUG_ENABLED` not defined), all debug output is
compiled out with zero overhead.

```cpp
// Enable debug output — place before #include <Loom.h>
#define LOOM_DEBUG_ENABLED
#include <Loom.h>
```

Actor names are stored as `const char*` string literals. On ARM, these reside
in flash automatically and are accessed transparently.

### Slow Handler Warning

In debug mode, Loom measures the execution time of each state handler call. If
a handler exceeds a configurable threshold (default: 10ms), a warning is
emitted:

```
[Loom] WARNING: Slow handler in actor=Display, state=stateRendering (23ms > 10ms threshold)
```

This helps catch accidentally-blocking code during development.

---

## 13. File & Library Structure

```
Loom/
├── library.properties          ← Arduino library metadata
├── README.md
├── LICENSE
│
├── src/
│   ├── Loom.h                  ← Main include (users only need this)
│   ├── LoomCore.h / .cpp       ← Actor registry, Bus, startup orchestration
│   ├── Actor.h                 ← Actor CRTP base class template
│   ├── Event.h                 ← Event struct, Signal type, payload accessors
│   ├── TimerManager.h / .cpp   ← FreeRTOS static timer wrapper
│   ├── Bus.h / .cpp            ← Publish / subscribe
│   └── Platform.h              ← Platform abstraction (GPIO, interrupt control)
│
└── examples/
    ├── Blinky/                 ← Single Actor, one Time Event. Hello World.
    │   └── Blinky.ino
    ├── ButtonLed/              ← Two Actors communicating via publish/subscribe
    │   └── ButtonLed.ino
    ├── TrafficLight/           ← Multi-state Actor with timer-driven transitions
    │   └── TrafficLight.ino
    └── SensorDisplay/          ← IMU Actor publishing to a Display Actor (CLUE)
        └── SensorDisplay.ino
```

---

## 14. Constraints, Limitations & Non-Goals

| Item | Decision |
|---|---|
| **Hierarchical state machines** | Not supported. Use multiple cooperating Actors instead. |
| **Dynamic Actor creation** | Not supported. All Actors are global instances, created before `Loom.begin()`. |
| **Actor-to-Actor synchronous calls** | Not supported. All communication is via asynchronous events. |
| **Code generation tooling** | Not planned. The framework is designed to be written by hand. |
| **C support** | C++ only. The Arduino ecosystem is C++; a C port adds complexity with little benefit. |
| **Networking / MQTT / WiFi** | Out of scope. Loom manages Actor concurrency. Networking is a user concern. |
| **BLE stack management** | Out of scope. Loom provides patterns for bridging BLE callbacks into events, but does not manage the BLE stack itself. |
| **Display rendering** | Out of scope. A Display Actor is a natural pattern, but Loom does not include graphics primitives. |
| **Multi-board support (v1)** | Not a goal for v1. The nRF52840 is the only supported target. The Platform abstraction exists to make future ports possible. |

---

## 15. Glossary

| Term | Definition |
|---|---|
| **Actor** | An autonomous unit of behavior with its own FreeRTOS task, event queue, and state machine. |
| **Signal** | A `uint16_t` identifier for the type of event. Defined in a user `enum`. |
| **Event** | An immutable message consisting of a signal and a 32-byte payload. |
| **Event Queue** | A statically allocated FreeRTOS queue owned by each Actor that holds pending events. |
| **State** | A member function of an Actor that handles events for a particular behavioral mode. |
| **State Handler** | The function that implements a state — receives events and may call `transitionTo()`. |
| **Transition** | The act of moving from one state to another, triggering EXIT and ENTRY events. |
| **Initial Transition** | The first `transitionTo()` call in `begin()`. Skips EXIT (no prior state), only sends ENTRY. |
| **Time Event** | A statically allocated FreeRTOS software timer that posts a specified signal to the owning Actor after a delay. |
| **Bus** | The global publish/subscribe channel. Any Actor may publish; subscribed Actors receive a copy. |
| **Run-to-Completion** | Each event is fully processed by one Actor before the next event is dequeued from that Actor's queue. |
| **ISR** | Interrupt Service Routine — hardware interrupt handler. Must not block; should only post events via `postFromISR()`. |
| **Task** | A FreeRTOS execution context. Each Actor runs in its own task. |
| **CRTP** | Curiously Recurring Template Pattern — the Actor base class is templated on the derived class for type-safe member function pointers. |

---

## 16. Open Questions

Most of the original design questions have been resolved. The following items
remain for discussion during implementation:

1. **Default stack size** — 512 bytes is a reasonable starting point for simple
   Actors, but Actors that do I2C sensor reads, display rendering, or BLE
   operations may need more. Should Loom provide predefined size constants
   (e.g., `LOOM_STACK_SMALL = 256`, `LOOM_STACK_MEDIUM = 512`,
   `LOOM_STACK_LARGE = 1024`) as a third template parameter on Actor?

2. **Queue-full policy** — Currently, a failed post silently drops the event
   and emits a debug warning. Should Loom offer alternative policies
   (overwrite oldest, block sender briefly, return error code) configurable per
   Actor?

3. **Testing harness** — The Platform abstraction is designed with testability
   in mind. A POSIX FreeRTOS port and hardware API stubs would enable host-PC
   testing. This is deferred to a later version, but the abstraction boundaries
   should be validated during initial implementation.
