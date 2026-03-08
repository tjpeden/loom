// ---------------------------------------------------------------------------
// PedestrianButtonActor — polls a button pin and publishes a pedestrian
// request event when the button is pressed.
// ---------------------------------------------------------------------------

#ifndef PEDESTRIAN_BUTTON_ACTOR_H
#define PEDESTRIAN_BUTTON_ACTOR_H

#include <Loom.h>
#include "signals.h"

class PedestrianButtonActor : public Actor<8, PedestrianButtonActor> {
public:
    explicit PedestrianButtonActor(uint8_t buttonPin)
        : Actor("PedBtn"), _buttonPin(buttonPin) {}

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

#endif // PEDESTRIAN_BUTTON_ACTOR_H
