// ---------------------------------------------------------------------------
// TrafficLight — Shared Signal Definitions
// ---------------------------------------------------------------------------

#ifndef TRAFFIC_LIGHT_SIGNALS_H
#define TRAFFIC_LIGHT_SIGNALS_H

#include <Loom.h>

enum Signal : uint16_t {
    SIGNAL_ENTRY = 0,
    SIGNAL_EXIT  = 1,

    SIGNAL_TIMEOUT,
    SIGNAL_PEDESTRIAN_REQUEST,
    SIGNAL_POLL_PIN,
};

#endif // TRAFFIC_LIGHT_SIGNALS_H
