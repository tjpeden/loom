// ---------------------------------------------------------------------------
// ReverseGeocache — Waypoint Store
// ---------------------------------------------------------------------------
// Loads waypoints from a JSON file on the QSPI flash (FAT filesystem).
// The CLUE's 2 MB QSPI flash is exposed as a USB mass storage drive,
// so the user can drag-and-drop waypoints.json from their computer.
//
// JSON format (waypoints.json):
//   {
//     "arrival_threshold": 0.0075,
//     "waypoints": [
//       { "lat": 40.748817, "lon": -73.985428, "name": "Start", "desc": "Begin!" },
//       ...
//     ]
//   }
//
// Static allocation: max 16 waypoints, loaded once at boot.
// ---------------------------------------------------------------------------

#ifndef GEOCACHE_WAYPOINTS_H
#define GEOCACHE_WAYPOINTS_H

#include <stdint.h>

struct Waypoint {
    double latitude;
    double longitude;
    char   name[16];
    char   description[32];
};

// ---------------------------------------------------------------------------
// WaypointStore — singleton, populated by loadFromFile() during setup()
// ---------------------------------------------------------------------------

class WaypointStore {
public:
    static constexpr uint8_t MAX_WAYPOINTS = 16;
    static constexpr double  DEFAULT_ARRIVAL_THRESHOLD = 0.0075; // miles (~40 ft)

    static bool loadFromFile();  // Call in setup() after FAT mount

    static uint8_t       count()    { return _count; }
    static const Waypoint& waypoint(uint8_t i) { return _waypoints[i]; }
    static double         arrivalThreshold() { return _arrivalThreshold; }
    static bool           loaded()  { return _loaded; }

private:
    static Waypoint _waypoints[MAX_WAYPOINTS];
    static uint8_t  _count;
    static double   _arrivalThreshold;
    static bool     _loaded;
};

// Call BEFORE Serial.begin() so MSC is included in the initial USB enumeration.
void initFlashMSC();

// Call AFTER Serial.begin() to verify FAT mount and print diagnostics.
// Returns true if FAT is mounted successfully.
bool mountFATFilesystem();

#endif // GEOCACHE_WAYPOINTS_H
