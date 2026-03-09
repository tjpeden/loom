// ---------------------------------------------------------------------------
// ReverseGeocache — NavActor (Navigation)
// ---------------------------------------------------------------------------
// Receives GPS fix events, computes distance and bearing to the current
// waypoint, and publishes navigation updates. Detects arrival when the
// distance drops below the threshold.
//
// States:
//   stateIdle       — not navigating
//   stateNavigating — receiving GPS fixes, computing nav data
//
// Responds to:
//   SIG_GPS_FIX    — compute distance/bearing, publish SIG_NAV_UPDATE
//   SIG_NAV_SET_WP — set current waypoint index (uint8_t payload)
//   SIG_NAV_STOP   — return to idle
//
// Publishes:
//   SIG_NAV_UPDATE  — NavData with distance, bearing, progress
//   SIG_NAV_ARRIVED — within threshold of current waypoint
// ---------------------------------------------------------------------------

#ifndef GEOCACHE_NAV_ACTOR_H
#define GEOCACHE_NAV_ACTOR_H

#include "Signals.h"
#include "Waypoints.h"
#include <math.h>

class NavActor : public Actor<8, NavActor, 512> {
public:
    NavActor() : Actor("Nav") {}

    void begin() override {
        transitionTo(&NavActor::stateIdle);
    }

private:
    uint8_t _currentWp = 0;

    // --- stateIdle -------------------------------------------------------

    void stateIdle(Event const& event) {
        switch (event.signal) {
            case SIG_NAV_SET_WP: {
                _currentWp = event.getPayload<uint8_t>();
                transitionTo(&NavActor::stateNavigating);
                break;
            }

            default:
                break;
        }
    }

    // --- stateNavigating -------------------------------------------------

    void stateNavigating(Event const& event) {
        switch (event.signal) {
            case SIG_GPS_FIX: {
                GpsData gps = event.getPayload<GpsData>();

                if (_currentWp >= WaypointStore::count()) break;

                const Waypoint& wp = WaypointStore::waypoint(_currentWp);

                double distMiles = haversineDistanceMiles(
                    gps.latitude, gps.longitude,
                    wp.latitude, wp.longitude
                );

                float bearing = computeBearing(
                    gps.latitude, gps.longitude,
                    wp.latitude, wp.longitude
                );

                // Publish nav update
                NavData nav;
                nav.distanceMiles = (float)distMiles;
                nav.bearing       = bearing;
                nav.wpIndex       = _currentWp;
                nav.wpTotal       = WaypointStore::count();

                Event navEvt = Event::make(SIG_NAV_UPDATE);
                navEvt.setPayload(nav);
                publishEvent(navEvt);

                // Check arrival
                if (distMiles < WaypointStore::arrivalThreshold()) {
                    publishEvent(Event::make(SIG_NAV_ARRIVED));
                }
                break;
            }

            case SIG_NAV_SET_WP: {
                _currentWp = event.getPayload<uint8_t>();
                break;
            }

            case SIG_NAV_STOP:
                transitionTo(&NavActor::stateIdle);
                break;

            default:
                break;
        }
    }

    // --- Navigation math -------------------------------------------------

    static constexpr double MY_DEG_TO_RAD = 3.14159265358979323846 / 180.0;
    static constexpr double MY_RAD_TO_DEG = 180.0 / 3.14159265358979323846;
    static constexpr double EARTH_RADIUS_MILES = 3958.8;

    static double haversineDistanceMiles(double lat1, double lon1,
                                          double lat2, double lon2) {
        double dLat = (lat2 - lat1) * MY_DEG_TO_RAD;
        double dLon = (lon2 - lon1) * MY_DEG_TO_RAD;
        double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
                   cos(lat1 * MY_DEG_TO_RAD) * cos(lat2 * MY_DEG_TO_RAD) *
                   sin(dLon / 2.0) * sin(dLon / 2.0);
        double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
        return EARTH_RADIUS_MILES * c;
    }

    static float computeBearing(double lat1, double lon1,
                                 double lat2, double lon2) {
        double dLon = (lon2 - lon1) * MY_DEG_TO_RAD;
        double y = sin(dLon) * cos(lat2 * MY_DEG_TO_RAD);
        double x = cos(lat1 * MY_DEG_TO_RAD) * sin(lat2 * MY_DEG_TO_RAD) -
                   sin(lat1 * MY_DEG_TO_RAD) * cos(lat2 * MY_DEG_TO_RAD) * cos(dLon);
        double bearing = atan2(y, x) * MY_RAD_TO_DEG;
        if (bearing < 0) bearing += 360.0;
        return (float)bearing;
    }

public:
    // Cardinal direction helper — static so DisplayActor can use it
    static const char* bearingToCardinal(float bearing) {
        if (bearing < 22.5  || bearing >= 337.5) return "N";
        if (bearing < 67.5)  return "NE";
        if (bearing < 112.5) return "E";
        if (bearing < 157.5) return "SE";
        if (bearing < 202.5) return "S";
        if (bearing < 247.5) return "SW";
        if (bearing < 292.5) return "W";
        return "NW";
    }
};

#endif // GEOCACHE_NAV_ACTOR_H
