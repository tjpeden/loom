// ---------------------------------------------------------------------------
// TrafficLight — Multi-state Actor with Timer-driven Transitions
// ---------------------------------------------------------------------------
// Demonstrates two cooperating Actors:
//   - TrafficLightActor: three states (Green, Yellow, Red) with timers
//   - PedestrianButtonActor: polls a button and publishes crossing requests
//
// The PedestrianButtonActor publishes SIGNAL_PEDESTRIAN_REQUEST via the Bus.
// The TrafficLightActor subscribes and records that a pedestrian is waiting.
//
// Target: Adafruit CLUE (nRF52840)
// ---------------------------------------------------------------------------

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
