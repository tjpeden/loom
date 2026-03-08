// ---------------------------------------------------------------------------
// ButtonLed — Two Actors Communicating via Publish/Subscribe
// ---------------------------------------------------------------------------
// Demonstrates the Bus (pub/sub) pattern with two Actors:
//   - ButtonActor: polls a button pin and publishes press/release events
//   - LedActor: subscribes to button events and toggles an LED
//
// The ButtonActor debounces a physical button and publishes
// SIGNAL_BUTTON_PRESSED when a press is detected. The LedActor subscribes
// to this signal and toggles its LED between on and off states.
//
// Target: Adafruit CLUE (nRF52840)
//   Button: pin 5 (left button on CLUE, active LOW with internal pull-up)
//   LED:    LED_BUILTIN (pin 17)
// ---------------------------------------------------------------------------

#include <Loom.h>

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

static constexpr Signal SIG_POLL_BUTTON    = SIGNAL_USER;
static constexpr Signal SIG_BUTTON_PRESSED = SIGNAL_USER + 1;

// ---------------------------------------------------------------------------
// ButtonActor — polls a button with debouncing, publishes press events
// ---------------------------------------------------------------------------

class ButtonActor : public Actor<8, ButtonActor> {
public:
    explicit ButtonActor(uint8_t buttonPin)
        : Actor("Button"), _buttonPin(buttonPin) {}

    void begin() override {
        pinMode(_buttonPin, INPUT_PULLUP);
        transitionTo(&ButtonActor::stateIdle);
    }

private:
    uint8_t _buttonPin;
    bool    _lastReading = true;  // pull-up: idle is HIGH

    // --- States ----------------------------------------------------------

    void stateIdle(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                startRepeatingTimer(SIG_POLL_BUTTON, 20);  // 20ms poll
                break;

            case SIG_POLL_BUTTON: {
                bool reading = digitalRead(_buttonPin);
                // Detect falling edge (HIGH -> LOW = press on active-low button)
                if (_lastReading == true && reading == false) {
                    publishEvent(Event::make(SIG_BUTTON_PRESSED));
                }
                _lastReading = reading;
                break;
            }

            case SIGNAL_EXIT:
                cancelTimer(SIG_POLL_BUTTON);
                break;

            default:
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// LedActor — toggles LED on each button press event
// ---------------------------------------------------------------------------

class LedActor : public Actor<8, LedActor> {
public:
    explicit LedActor(uint8_t ledPin)
        : Actor("Led"), _ledPin(ledPin) {}

    void begin() override {
        pinMode(_ledPin, OUTPUT);
        transitionTo(&LedActor::stateOff);
    }

private:
    uint8_t _ledPin;

    // --- States ----------------------------------------------------------

    void stateOff(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                digitalWrite(_ledPin, LOW);
                break;

            case SIG_BUTTON_PRESSED:
                transitionTo(&LedActor::stateOn);
                break;

            default:
                break;
        }
    }

    void stateOn(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                digitalWrite(_ledPin, HIGH);
                break;

            case SIG_BUTTON_PRESSED:
                transitionTo(&LedActor::stateOff);
                break;

            default:
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// Sketch
// ---------------------------------------------------------------------------

ButtonActor button(5);            // CLUE left button
LedActor    led(LED_BUILTIN);     // Onboard LED

void setup() {
    Serial.begin(115200);

    Loom.registerActor(button, /*priority=*/1);
    Loom.registerActor(led,    /*priority=*/1);

    // LedActor subscribes to button press events via the Bus
    Loom.subscribe(SIG_BUTTON_PRESSED, led);

    Loom.begin();
}

void loop() {
    // Empty — all work happens in Actor tasks
}
