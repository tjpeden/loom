// ---------------------------------------------------------------------------
// ReverseGeocache — Signal Definitions
// ---------------------------------------------------------------------------
// All signals used across the system. Framework reserves 0 (ENTRY) and
// 1 (EXIT). User signals start at SIGNAL_USER (2).
// ---------------------------------------------------------------------------

#ifndef GEOCACHE_SIGNALS_H
#define GEOCACHE_SIGNALS_H

#include <Loom.h>

// ---------------------------------------------------------------------------
// Published signals (travel via Bus)
// ---------------------------------------------------------------------------

// InputActor → all subscribers
static constexpr Signal SIG_BUTTON_A_PRESS = SIGNAL_USER;       // Left button short press
static constexpr Signal SIG_BUTTON_B_PRESS = SIGNAL_USER + 1;   // Right button short press
static constexpr Signal SIG_BUTTON_A_HOLD  = SIGNAL_USER + 2;   // Left button long hold (2s)

// GpsActor → NavActor, DisplayActor
static constexpr Signal SIG_GPS_FIX        = SIGNAL_USER + 3;   // Valid GPS fix (GpsData payload)
static constexpr Signal SIG_GPS_SATS       = SIGNAL_USER + 4;   // Satellite count update (uint8_t)
static constexpr Signal SIG_GPS_NO_FIX     = SIGNAL_USER + 5;   // GPS timed out, no fix

// NavActor → DisplayActor, BoxActor
static constexpr Signal SIG_NAV_UPDATE     = SIGNAL_USER + 6;   // Distance/bearing update (NavData)
static constexpr Signal SIG_NAV_ARRIVED    = SIGNAL_USER + 7;   // Within threshold of waypoint

// BoxActor → DisplayActor
static constexpr Signal SIG_BOX_STATE      = SIGNAL_USER + 8;   // Box state change (BoxState)
static constexpr Signal SIG_WAYPOINT_INFO  = SIGNAL_USER + 9;   // Current waypoint info (WaypointInfo)

// ---------------------------------------------------------------------------
// Command signals (sent directly, not via Bus)
// ---------------------------------------------------------------------------

static constexpr Signal SIG_GPS_START      = SIGNAL_USER + 10;  // BoxActor → GpsActor: start reading
static constexpr Signal SIG_GPS_STOP       = SIGNAL_USER + 11;  // BoxActor → GpsActor: stop reading
static constexpr Signal SIG_NAV_SET_WP     = SIGNAL_USER + 12;  // BoxActor → NavActor: set waypoint index
static constexpr Signal SIG_NAV_STOP       = SIGNAL_USER + 13;  // BoxActor → NavActor: stop navigating

// ---------------------------------------------------------------------------
// Internal timer signals (not published)
// ---------------------------------------------------------------------------

static constexpr Signal SIG_POLL_BUTTONS   = SIGNAL_USER + 14;
static constexpr Signal SIG_READ_GPS       = SIGNAL_USER + 15;
static constexpr Signal SIG_GPS_TIMEOUT    = SIGNAL_USER + 16;
static constexpr Signal SIG_SLEEP_TIMEOUT  = SIGNAL_USER + 17;
static constexpr Signal SIG_TONE_DONE      = SIGNAL_USER + 18;
static constexpr Signal SIG_SERVO_DONE     = SIGNAL_USER + 19;

// ---------------------------------------------------------------------------
// Payload structs — all must fit within 32-byte Event payload
// ---------------------------------------------------------------------------

struct GpsData {
    double latitude;     // 8 bytes
    double longitude;    // 8 bytes
    uint8_t satellites;  // 1 byte
    // Total: 17 bytes — fits
};

struct NavData {
    float distanceMiles; // 4 bytes
    float bearing;       // 4 bytes (degrees, 0-360)
    uint8_t wpIndex;     // 1 byte — current waypoint index
    uint8_t wpTotal;     // 1 byte — total waypoints
    // Total: 10 bytes — fits
};

enum class BoxState : uint8_t {
    Loading,
    Unarmed,
    Arming,        // Locking servo
    Acquiring,     // Waiting for GPS fix
    Navigating,    // Have fix, showing distance/bearing
    Checkpoint,    // Arrived at waypoint
    OpenBox,       // Final destination reached
    GpsFail,       // GPS acquisition failed
    Sleeping,      // Low-power sleep mode
};

struct BoxStatePayload {
    BoxState state;
};

struct WaypointInfo {
    uint8_t index;         // 1 byte
    uint8_t total;         // 1 byte
    char    name[16];      // 16 bytes
    char    desc[14];      // 14 bytes (truncated for payload)
    // Total: 32 bytes — exact fit
};

#endif // GEOCACHE_SIGNALS_H
