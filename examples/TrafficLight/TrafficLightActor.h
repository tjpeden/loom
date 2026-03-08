// ---------------------------------------------------------------------------
// TrafficLightActor — three-state traffic light driven by timers.
// Responds to pedestrian crossing requests by shortening the green phase.
// ---------------------------------------------------------------------------

#ifndef TRAFFIC_LIGHT_ACTOR_H
#define TRAFFIC_LIGHT_ACTOR_H

#include <Loom.h>
#include "signals.h"

class TrafficLightActor : public Actor<8, TrafficLightActor> {
public:
    TrafficLightActor(uint8_t redPin, uint8_t yellowPin, uint8_t greenPin)
        : Actor("Traffic"),
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

#endif // TRAFFIC_LIGHT_ACTOR_H
