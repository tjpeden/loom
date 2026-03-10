# Loom

An event-driven, real-time embedded framework for Arduino using the Actor model
and flat finite state machines, backed by FreeRTOS.

Loom brings structured concurrency to hobbyist embedded projects. Each Actor
runs in its own FreeRTOS task, communicates exclusively through events, and
manages its own flat state machine. No `delay()`, no shared mutable state, no
dynamic allocation.

## Target Platform

**nRF52840** — specifically the [Adafruit CLUE](https://www.adafruit.com/product/4500)
running the Adafruit nRF52 Arduino core (Bluefruit).

| Spec        | Value                           |
|-------------|---------------------------------|
| MCU         | Nordic nRF52840 (Cortex-M4F)    |
| Clock       | 64 MHz                          |
| RAM         | 256 KB                          |
| Flash       | 1 MB                            |
| RTOS        | FreeRTOS (bundled with Adafruit BSP) |

## Core Concepts

**Actors** are autonomous units of behavior. Each Actor owns a FreeRTOS task,
an event queue, a current state, and private data. Actors never call each
other's methods — they communicate only through events.

**Events** are small value-type messages: a `uint16_t` signal plus a 32-byte
payload. They are copied into queues, not heap-allocated.

**States** are member functions with the signature `void(Event const&)`.
Transitions send `SIGNAL_EXIT` to the old state and `SIGNAL_ENTRY` to the new
one.

**Timers** replace `delay()`. An Actor can run multiple concurrent software
timers (one-shot or repeating) that post events to its own queue when they
fire.

**The Bus** provides publish/subscribe. Actors subscribe to signals at startup;
any Actor can publish an event and all subscribers receive a copy.

## Quick Start

```cpp
#include <Loom.h>

static constexpr Signal SIG_TIMEOUT = SIGNAL_USER;

class BlinkyActor : public Actor<8, BlinkyActor> {
public:
    explicit BlinkyActor(uint8_t pin)
        : Actor("Blinky"), _pin(pin) {}

    void begin() override {
        pinMode(_pin, OUTPUT);
        transitionTo(&BlinkyActor::stateLedOff);
    }

private:
    uint8_t _pin;

    void stateLedOff(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                digitalWrite(_pin, LOW);
                startTimer(SIG_TIMEOUT, 500);
                break;
            case SIG_TIMEOUT:
                transitionTo(&BlinkyActor::stateLedOn);
                break;
            case SIGNAL_EXIT:
                cancelTimer(SIG_TIMEOUT);
                break;
            default: break;
        }
    }

    void stateLedOn(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                digitalWrite(_pin, HIGH);
                startTimer(SIG_TIMEOUT, 500);
                break;
            case SIG_TIMEOUT:
                transitionTo(&BlinkyActor::stateLedOff);
                break;
            case SIGNAL_EXIT:
                cancelTimer(SIG_TIMEOUT);
                break;
            default: break;
        }
    }
};

BlinkyActor blinky(LED_BUILTIN);

void setup() {
    Loom.registerActor(blinky, /*priority=*/1);
    Loom.begin();
}

void loop() {
    // Empty — BlinkyActor runs in its own FreeRTOS task
}
```

## Architecture

```
 User Sketch
     │
     ▼
 Loom Core ─── Bus (pub/sub) ─── Actor Registry ─── Debug (Serial)
     │
     ▼
 Actor Tasks ── each with its own queue, state machine, timers
     │
     ▼
 FreeRTOS ──── preemptive scheduler, queues, software timers
     │
     ▼
 Hardware ──── GPIO, I2C, SPI, BLE
```

## Design Principles

- **Fully static allocation.** All memory is allocated at compile time. Actor
  objects embed their FreeRTOS stack, queue storage, and timer slots. The
  compiler memory report reflects actual usage. No `malloc`/`new` ever.

- **No blocking.** State handlers must never call `delay()` or spin-wait. Use
  timers instead.

- **Run-to-completion.** Each event is fully processed before the next is
  dequeued. Within an Actor, event handling is always sequential.

- **No shared mutable state.** Actors communicate only through events. No
  mutexes needed within an Actor.

- **CRTP-based Actor class.** State handlers are pointer-to-member-functions of
  the derived class, with zero type-erasure overhead:
  ```cpp
  class MyActor : public Actor<QueueDepth, MyActor, StackSize> { ... };
  ```

## Examples

| Example | Description |
|---------|-------------|
| [Blinky](examples/Blinky/) | Hello world — one Actor, two states, one timer |
| [ButtonLed](examples/ButtonLed/) | Two Actors communicating via the Bus |
| [TrafficLight](examples/TrafficLight/) | Multi-file project with three states |
| [SensorDisplay](examples/SensorDisplay/) | I2C sensor reads + TFT rendering |
| [ReverseGeocache](examples/ReverseGeocache/) | Full project — GPS puzzle box with 5 Actors, QSPI flash waypoints, USB mass storage |

## Memory Footprint

Per-Actor memory with an 8-slot queue, 4 timer slots, and 512-byte stack is
approximately 1.3 KB. A 10-Actor system plus framework overhead fits in about
14.3 KB (5.5% of 256 KB RAM).

The ReverseGeocache example (5 Actors, GPS, TFT, servo, QSPI flash) compiles
to 22% flash and 13% RAM.

## Installation

Clone or download this repository into your Arduino libraries folder, or use
the `--library` flag with `arduino-cli`:

```bash
arduino-cli compile \
  --fqbn adafruit:nrf52:cluenrf52840 \
  --library /path/to/loom \
  examples/Blinky
```

### Board Support

Install the Adafruit nRF52 board package via the Arduino Board Manager:

1. Add `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`
   to your board manager URLs
2. Install **Adafruit nRF52** from the Board Manager
3. Select **Adafruit CLUE** as your board

## Project Structure

```
loom/
├── src/
│   ├── Loom.h            # Main include
│   ├── LoomCore.h/cpp    # Framework core (registry, startup)
│   ├── Actor.h/cpp       # CRTP Actor base class
│   ├── Bus.h/cpp         # Publish/subscribe bus
│   ├── Event.h           # Event struct, Signal type, reserved signals
│   └── Platform.h        # Platform abstraction (FreeRTOS wrappers)
├── examples/
│   ├── Blinky/
│   ├── ButtonLed/
│   ├── TrafficLight/
│   ├── SensorDisplay/
│   └── ReverseGeocache/
├── DESIGN.md             # Full design document (v0.2.1)
├── library.properties
└── README.md
```

## Inspiration

Loom draws conceptual inspiration from [QP by Quantum Leaps](https://www.state-machine.com/)
— the gold standard of event-driven embedded frameworks. Loom trades QP's
power (hierarchical UML statecharts, modeling tools) for accessibility: flat
state machines, pure code, and a gentle learning curve.

## License

MIT
