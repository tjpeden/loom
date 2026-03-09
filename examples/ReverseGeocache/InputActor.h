// ---------------------------------------------------------------------------
// ReverseGeocache — InputActor
// ---------------------------------------------------------------------------
// Polls CLUE's built-in buttons (A=pin 5, B=pin 11) at 20Hz with software
// debounce. Publishes SIG_BUTTON_A_PRESS, SIG_BUTTON_B_PRESS on short press,
// and SIG_BUTTON_A_HOLD on 2-second hold of button A.
//
// Both CLUE buttons are active-LOW with internal pull-ups.
// ---------------------------------------------------------------------------

#ifndef GEOCACHE_INPUT_ACTOR_H
#define GEOCACHE_INPUT_ACTOR_H

#include "Signals.h"

class InputActor : public Actor<4, InputActor, 512> {
public:
    InputActor() : Actor("Input") {}

    void begin() override {
        pinMode(BUTTON_A_PIN, INPUT_PULLUP);
        pinMode(BUTTON_B_PIN, INPUT_PULLUP);

        transitionTo(&InputActor::statePolling);
    }

private:
    static constexpr uint8_t BUTTON_A_PIN = 5;
    static constexpr uint8_t BUTTON_B_PIN = 11;

    static constexpr uint32_t POLL_INTERVAL_MS  = 50;   // 20 Hz
    static constexpr uint32_t DEBOUNCE_MS       = 50;   // debounce time
    static constexpr uint32_t HOLD_THRESHOLD_MS = 2000; // long hold

    // Button state tracking
    struct ButtonState {
        bool     lastRaw       = true;   // active-LOW: true = released
        bool     stable        = false;  // debounced pressed state (false = released)
        uint32_t lastChangeMs  = 0;      // last raw-state change time
        uint32_t pressStartMs  = 0;      // when stable went pressed
        bool     holdFired     = false;  // long-hold already reported?
    };

    ButtonState _btnA;
    ButtonState _btnB;

    void statePolling(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                startRepeatingTimer(SIG_POLL_BUTTONS, POLL_INTERVAL_MS);
                break;

            case SIG_POLL_BUTTONS: {
                uint32_t now = loomMillis();
                processButton(_btnA, digitalRead(BUTTON_A_PIN), now,
                              SIG_BUTTON_A_PRESS, SIG_BUTTON_A_HOLD);
                processButton(_btnB, digitalRead(BUTTON_B_PIN), now,
                              SIG_BUTTON_B_PRESS, 0);  // no hold for B
                break;
            }

            case SIGNAL_EXIT:
                cancelTimer(SIG_POLL_BUTTONS);
                break;

            default:
                break;
        }
    }

    void processButton(ButtonState& btn, bool rawReading, uint32_t now,
                        Signal pressSignal, Signal holdSignal) {
        // Detect raw state change for debounce timing
        if (rawReading != btn.lastRaw) {
            btn.lastRaw = rawReading;
            btn.lastChangeMs = now;
        }

        // Wait for debounce period
        if ((now - btn.lastChangeMs) < DEBOUNCE_MS) return;

        bool pressed = !rawReading;  // active-LOW

        // Transition: released → pressed
        if (pressed && !btn.stable) {
            btn.stable = true;
            btn.pressStartMs = now;
            btn.holdFired = false;
        }
        // Transition: pressed → released
        else if (!pressed && btn.stable) {
            btn.stable = false;
            // Only fire press if hold wasn't already fired
            if (!btn.holdFired && pressSignal != 0) {
                publishEvent(Event::make(pressSignal));
            }
        }
        // Held: check for long hold
        else if (pressed && btn.stable && !btn.holdFired && holdSignal != 0) {
            if ((now - btn.pressStartMs) >= HOLD_THRESHOLD_MS) {
                btn.holdFired = true;
                publishEvent(Event::make(holdSignal));
            }
        }
    }
};

#endif // GEOCACHE_INPUT_ACTOR_H
