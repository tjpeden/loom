// ---------------------------------------------------------------------------
// TrafficLight — Shared Signal Definitions
// ---------------------------------------------------------------------------

#ifndef TRAFFIC_LIGHT_SIGNALS_H
#define TRAFFIC_LIGHT_SIGNALS_H

#include <Loom.h>

static constexpr Signal SIGNAL_TIMEOUT             = SIGNAL_USER;
static constexpr Signal SIGNAL_PEDESTRIAN_REQUEST   = SIGNAL_USER + 1;
static constexpr Signal SIGNAL_POLL_PIN             = SIGNAL_USER + 2;

#endif // TRAFFIC_LIGHT_SIGNALS_H
