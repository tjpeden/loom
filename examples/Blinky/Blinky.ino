// ---------------------------------------------------------------------------
// Blinky — Loom Hello World
// ---------------------------------------------------------------------------
// A single Actor with two states (LedOn and LedOff) that blink the onboard
// LED using a repeating timer. No delay(), no millis() — just events.
//
// Target: Adafruit CLUE (nRF52840)
// LED pin: LED_BUILTIN (pin 17 on CLUE)
// ---------------------------------------------------------------------------

#include <Loom.h>

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

static constexpr Signal SIG_TIMEOUT = SIGNAL_USER;

// ---------------------------------------------------------------------------
// BlinkyActor
// ---------------------------------------------------------------------------

class BlinkyActor : public Actor<8, BlinkyActor> {
public:
    explicit BlinkyActor(uint8_t ledPin)
        : Actor("Blinky"), _ledPin(ledPin) {}

    void begin() override {
        pinMode(_ledPin, OUTPUT);
        transitionTo(&BlinkyActor::stateLedOff);
    }

private:
    uint8_t _ledPin;

    // --- States ----------------------------------------------------------

    void stateLedOff(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                digitalWrite(_ledPin, LOW);
                startTimer(SIG_TIMEOUT, 500);  // 500ms off
                break;

            case SIG_TIMEOUT:
                transitionTo(&BlinkyActor::stateLedOn);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIG_TIMEOUT);
                break;

            default:
                break;
        }
    }

    void stateLedOn(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                digitalWrite(_ledPin, HIGH);
                startTimer(SIG_TIMEOUT, 500);  // 500ms on
                break;

            case SIG_TIMEOUT:
                transitionTo(&BlinkyActor::stateLedOff);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIG_TIMEOUT);
                break;

            default:
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// Sketch
// ---------------------------------------------------------------------------

BlinkyActor blinky(LED_BUILTIN);

void setup() {
    Serial.begin(115200);

    Loom.registerActor(blinky, /*priority=*/1);
    Loom.begin();
}

void loop() {
    // Empty — BlinkyActor runs in its own FreeRTOS task
}
