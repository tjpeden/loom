// ---------------------------------------------------------------------------
// ReverseGeocache — BoxActor
// ---------------------------------------------------------------------------
// Central coordinator for the geocache box. Manages the servo lock/unlock
// mechanism, armed/unarmed state, waypoint progression, and persists
// state to nRF52 InternalFileSystem (LittleFS).
//
// States:
//   stateLoading    — reads persisted state, decides next state
//   stateUnarmed    — box unlocked, waiting for hold to arm
//   stateArming     — locking the servo
//   stateAcquiring  — GPS acquiring a fix
//   stateNavigating — have a fix, showing distance/bearing
//   stateCheckpoint — arrived at a waypoint, showing description
//   stateOpenBox    — final waypoint reached, box unlocked
//   stateGpsFail    — GPS timed out
//   stateSleeping   — display off, GPS off, waiting for button
//
// The BoxActor owns the servo and buzzer pins. It sends direct commands
// to GpsActor (start/stop) and NavActor (set waypoint / stop).
// ---------------------------------------------------------------------------

#ifndef GEOCACHE_BOX_ACTOR_H
#define GEOCACHE_BOX_ACTOR_H

#include "Signals.h"
#include "Waypoints.h"
#include <Servo.h>
#include <InternalFileSystem.h>

// Forward declare so we can send direct commands
extern class GpsActor  gpsActor;
extern class NavActor  navActor;

class BoxActor : public Actor<8, BoxActor, 1024> {
public:
    BoxActor() : Actor("Box") {}

    void begin() override {
        // Initialize servo pin (but don't attach yet — only during movement)
        pinMode(SERVO_PIN, OUTPUT);

        // Initialize buzzer
        pinMode(BUZZER_PIN, OUTPUT);

        // Initialize InternalFileSystem for state persistence
        InternalFS.begin();

        // Load persisted state
        _loadState();

        transitionTo(&BoxActor::stateLoading);
    }

private:
    static constexpr uint8_t SERVO_PIN  = 2;   // External servo via GPIO
    static constexpr uint8_t BUZZER_PIN = 46;   // CLUE onboard buzzer

    static constexpr uint16_t SERVO_LOCKED_POS   = 150;
    static constexpr uint16_t SERVO_UNLOCKED_POS = 180;
    static constexpr uint32_t SERVO_MOVE_TIME_MS = 1000;

    static constexpr uint32_t SLEEP_TIMEOUT_MS = 5UL * 60UL * 1000UL; // 5 minutes

    // Persisted state
    uint8_t _waypointIndex = 0;
    bool    _armed         = false;

    // Timing for reset guard
    uint32_t _stateEntryMs = 0;

    Servo _servo;

    // --- Melody sequencer (non-blocking) ---------------------------------

    struct Note {
        uint16_t freq;
        uint16_t durationMs;
        uint16_t pauseMs;   // gap after note before next note
    };

    static const Note ARRIVAL_MELODY[];
    static const uint8_t ARRIVAL_MELODY_LEN = 3;

    static const Note VICTORY_MELODY[];
    static const uint8_t VICTORY_MELODY_LEN = 4;

    const Note* _melodyNotes  = nullptr;
    uint8_t     _melodyLen    = 0;
    uint8_t     _melodyIndex  = 0;

    void _startMelody(const Note* notes, uint8_t len) {
        _melodyNotes = notes;
        _melodyLen   = len;
        _melodyIndex = 0;
        _playNextNote();
    }

    void _playNextNote() {
        if (_melodyIndex >= _melodyLen) {
            _melodyNotes = nullptr;  // done
            return;
        }
        const Note& n = _melodyNotes[_melodyIndex];
        tone(BUZZER_PIN, n.freq, n.durationMs);
        // Timer fires after note duration + pause gap
        startTimer(SIG_TONE_DONE, n.durationMs + n.pauseMs);
        _melodyIndex++;
    }

    // --- Persistence (InternalFileSystem) --------------------------------

    static constexpr const char* STATE_FILENAME = "/geocache.dat";

    struct PersistedState {
        uint8_t waypointIndex;
        uint8_t armed;  // 0 or 1
    };

    void _loadState() {
        Adafruit_LittleFS_Namespace::File file(InternalFS);
        if (file.open(STATE_FILENAME, Adafruit_LittleFS_Namespace::FILE_O_READ)) {
            PersistedState ps;
            if (file.read(reinterpret_cast<uint8_t*>(&ps), sizeof(ps)) == sizeof(ps)) {
                _waypointIndex = ps.waypointIndex;
                _armed = (ps.armed != 0);
                // Guard against corrupted flash
                if (_waypointIndex > WaypointStore::count()) {
                    _waypointIndex = 0;
                    _armed = false;
                }
                LOOM_LOG("Loaded state: wp=%u, armed=%u", _waypointIndex, _armed);
            }
            file.close();
        } else {
            LOOM_LOG("No saved state found, starting fresh");
        }
    }

    void _saveState() {
        Adafruit_LittleFS_Namespace::File file(InternalFS);
        if (file.open(STATE_FILENAME, Adafruit_LittleFS_Namespace::FILE_O_WRITE)) {
            file.truncate(0);
            PersistedState ps;
            ps.waypointIndex = _waypointIndex;
            ps.armed = _armed ? 1 : 0;
            file.write(reinterpret_cast<const uint8_t*>(&ps), sizeof(ps));
            file.close();
        }
    }

    // --- Publish box state -----------------------------------------------

    void _publishState(BoxState state) {
        Event evt = Event::make(SIG_BOX_STATE);
        BoxStatePayload payload;
        payload.state = state;
        evt.setPayload(payload);
        publishEvent(evt);
    }

    // --- Publish waypoint info -------------------------------------------

    void _publishWaypointInfo() {
        if (_waypointIndex >= WaypointStore::count()) return;

        const Waypoint& wp = WaypointStore::waypoint(_waypointIndex);
        WaypointInfo info;
        info.index = _waypointIndex;
        info.total = WaypointStore::count();

        // Copy name (truncate to fit)
        strncpy(info.name, wp.name, sizeof(info.name) - 1);
        info.name[sizeof(info.name) - 1] = '\0';

        // Copy description (truncate to fit)
        strncpy(info.desc, wp.description, sizeof(info.desc) - 1);
        info.desc[sizeof(info.desc) - 1] = '\0';

        Event evt = Event::make(SIG_WAYPOINT_INFO);
        evt.setPayload(info);
        publishEvent(evt);
    }

    // --- Servo helpers ---------------------------------------------------

    void _lockServo() {
        _servo.attach(SERVO_PIN);
        _servo.write(SERVO_LOCKED_POS);
        startTimer(SIG_SERVO_DONE, SERVO_MOVE_TIME_MS);
    }

    void _unlockServo() {
        _servo.attach(SERVO_PIN);
        _servo.write(SERVO_UNLOCKED_POS);
        startTimer(SIG_SERVO_DONE, SERVO_MOVE_TIME_MS);
    }

    void _detachServo() {
        _servo.detach();
    }

    // --- Buzzer helpers --------------------------------------------------

    void _playTone(uint16_t freq, uint32_t durationMs) {
        tone(BUZZER_PIN, freq, durationMs);
    }

    void _playArrivalMelody() {
        _startMelody(ARRIVAL_MELODY, ARRIVAL_MELODY_LEN);
    }

    void _playVictoryMelody() {
        _startMelody(VICTORY_MELODY, VICTORY_MELODY_LEN);
    }

    // --- Start GPS + Nav -------------------------------------------------

    void _startGpsAndNav() {
        // Tell NavActor which waypoint to navigate to
        Event wpEvt = Event::make(SIG_NAV_SET_WP);
        wpEvt.setPayload(_waypointIndex);
        navActor.postEvent(wpEvt);

        // Tell GpsActor to start reading
        gpsActor.postEvent(Event::make(SIG_GPS_START));
    }

    void _stopGpsAndNav() {
        gpsActor.postEvent(Event::make(SIG_GPS_STOP));
        navActor.postEvent(Event::make(SIG_NAV_STOP));
    }

    // =====================================================================
    // State handlers
    // =====================================================================

    // --- stateLoading ----------------------------------------------------

    void stateLoading(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY: {
                _publishState(BoxState::Loading);

                if (_armed) {
                    if (_waypointIndex >= WaypointStore::count()) {
                        // Already completed all waypoints
                        transitionTo(&BoxActor::stateOpenBox);
                    } else {
                        // Resume: start GPS acquisition
                        transitionTo(&BoxActor::stateAcquiring);
                    }
                } else {
                    transitionTo(&BoxActor::stateUnarmed);
                }
                break;
            }

            default:
                break;
        }
    }

    // --- stateUnarmed ----------------------------------------------------

    void stateUnarmed(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _armed = false;
                _unlockServo();
                _publishState(BoxState::Unarmed);
                break;

            case SIG_SERVO_DONE:
                _detachServo();
                break;

            case SIG_BUTTON_A_HOLD:
                // Arm the box
                transitionTo(&BoxActor::stateArming);
                break;

            case SIGNAL_EXIT:
                break;

            default:
                break;
        }
    }

    // --- stateArming -----------------------------------------------------

    void stateArming(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _armed = true;
                _waypointIndex = 0;
                _saveState();
                _lockServo();
                _publishState(BoxState::Arming);
                _playTone(262, 500);
                break;

            case SIG_SERVO_DONE:
                _detachServo();
                transitionTo(&BoxActor::stateAcquiring);
                break;

            default:
                break;
        }
    }

    // --- stateAcquiring --------------------------------------------------

    void stateAcquiring(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _publishState(BoxState::Acquiring);
                _startGpsAndNav();
                _stateEntryMs = loomMillis();
                startTimer(SIG_SLEEP_TIMEOUT, SLEEP_TIMEOUT_MS);
                break;

            case SIG_GPS_FIX:
                // We have a fix — move to navigating
                transitionTo(&BoxActor::stateNavigating);
                break;

            case SIG_GPS_NO_FIX:
                transitionTo(&BoxActor::stateGpsFail);
                break;

            case SIG_SLEEP_TIMEOUT:
                transitionTo(&BoxActor::stateSleeping);
                break;

            case SIG_BUTTON_A_HOLD:
                // Allow reset during first 20 seconds of this state
                if ((loomMillis() - _stateEntryMs) < 20000) {
                    _waypointIndex = 0;
                    _armed = false;
                    _saveState();
                    transitionTo(&BoxActor::stateUnarmed);
                }
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIG_SLEEP_TIMEOUT);
                break;

            default:
                break;
        }
    }

    // --- stateNavigating -------------------------------------------------

    void stateNavigating(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _publishState(BoxState::Navigating);
                _publishWaypointInfo();
                startTimer(SIG_SLEEP_TIMEOUT, SLEEP_TIMEOUT_MS);
                break;

            case SIG_NAV_ARRIVED:
                transitionTo(&BoxActor::stateCheckpoint);
                break;

            case SIG_SLEEP_TIMEOUT:
                transitionTo(&BoxActor::stateSleeping);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIG_SLEEP_TIMEOUT);
                break;

            default:
                break;
        }
    }

    // --- stateCheckpoint -------------------------------------------------

    void stateCheckpoint(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY: {
                _stopGpsAndNav();
                _playArrivalMelody();

                _publishState(BoxState::Checkpoint);
                _publishWaypointInfo();

                // Advance waypoint
                _waypointIndex++;
                _saveState();
                break;
            }

            case SIG_TONE_DONE:
                _playNextNote();
                break;

            case SIG_BUTTON_A_PRESS:
                if (_waypointIndex >= WaypointStore::count()) {
                    // That was the last waypoint!
                    transitionTo(&BoxActor::stateOpenBox);
                } else {
                    // More waypoints — go back to acquiring
                    transitionTo(&BoxActor::stateAcquiring);
                }
                break;

            default:
                break;
        }
    }

    // --- stateOpenBox ----------------------------------------------------

    void stateOpenBox(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _armed = false;
                _saveState();
                _unlockServo();
                _playVictoryMelody();
                _publishState(BoxState::OpenBox);
                break;

            case SIG_SERVO_DONE:
                _detachServo();
                break;

            case SIG_TONE_DONE:
                _playNextNote();
                break;

            case SIG_BUTTON_A_HOLD:
                // Reset — start over
                _waypointIndex = 0;
                _saveState();
                transitionTo(&BoxActor::stateUnarmed);
                break;

            default:
                break;
        }
    }

    // --- stateGpsFail ----------------------------------------------------

    void stateGpsFail(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _stopGpsAndNav();
                _publishState(BoxState::GpsFail);
                startTimer(SIG_SLEEP_TIMEOUT, SLEEP_TIMEOUT_MS);
                break;

            case SIG_BUTTON_A_PRESS:
                // Retry
                transitionTo(&BoxActor::stateAcquiring);
                break;

            case SIG_SLEEP_TIMEOUT:
                transitionTo(&BoxActor::stateSleeping);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIG_SLEEP_TIMEOUT);
                break;

            default:
                break;
        }
    }

    // --- stateSleeping ---------------------------------------------------

    void stateSleeping(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _stopGpsAndNav();
                _publishState(BoxState::Sleeping);
                // Display will turn off backlight when it sees this state
                break;

            case SIG_BUTTON_A_PRESS:
                // Wake up and resume
                if (_armed) {
                    transitionTo(&BoxActor::stateAcquiring);
                } else {
                    transitionTo(&BoxActor::stateUnarmed);
                }
                break;

            default:
                break;
        }
    }
};

// Out-of-class definitions for static const arrays (required pre-C++17)
const BoxActor::Note BoxActor::ARRIVAL_MELODY[] = {
    {262, 250, 50},   // C4
    {294, 250, 50},   // D4
    {330, 250, 0},    // E4 (last note, no pause)
};

const BoxActor::Note BoxActor::VICTORY_MELODY[] = {
    {262, 200, 50},   // C4
    {330, 200, 50},   // E4
    {392, 200, 50},   // G4
    {523, 400, 0},    // C5 (last note)
};

#endif // GEOCACHE_BOX_ACTOR_H
