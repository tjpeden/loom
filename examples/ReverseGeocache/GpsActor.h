// ---------------------------------------------------------------------------
// ReverseGeocache — GpsActor
// ---------------------------------------------------------------------------
// Manages the Adafruit Mini GPS PA1010D module over I2C (address 0x10).
// Uses the Adafruit GPS library for NMEA parsing.
//
// States:
//   stateIdle      — GPS not being read, low power
//   stateAcquiring — Reading GPS at 1Hz, publishing fix/satellite data
//
// Responds to direct commands:
//   SIG_GPS_START  — transition to Acquiring
//   SIG_GPS_STOP   — transition to Idle
//
// Publishes:
//   SIG_GPS_FIX    — valid fix with GpsData payload
//   SIG_GPS_SATS   — satellite count (uint8_t payload)
//   SIG_GPS_NO_FIX — fired after 30s timeout with no valid fix
// ---------------------------------------------------------------------------

#ifndef GEOCACHE_GPS_ACTOR_H
#define GEOCACHE_GPS_ACTOR_H

#include "Signals.h"
#include <Adafruit_GPS.h>
#include <Wire.h>

class GpsActor : public Actor<8, GpsActor, 1024> {
public:
    GpsActor() : Actor("Gps"), _gps(&Wire) {}

    void begin() override {
        // Initialize I2C GPS module
        if (!_gps.begin(0x10)) {
            LOOM_WARN("PA1010D GPS init failed!");
        }

        // Configure GPS: RMC+GGA sentences, 1Hz update
        _gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
        _gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
        _gps.sendCommand(PGCMD_ANTENNA);  // Request antenna status

        transitionTo(&GpsActor::stateIdle);
    }

private:
    Adafruit_GPS _gps;
    bool _hadFix = false;  // Track fix state for loss detection

    static constexpr uint32_t GPS_READ_INTERVAL_MS = 1000;  // 1 Hz
    static constexpr uint32_t GPS_TIMEOUT_MS       = 30000; // 30s to get fix

    // --- stateIdle: GPS module idle, not reading -------------------------

    void stateIdle(Event const& event) {
        switch (event.signal) {
            case SIG_GPS_START:
                transitionTo(&GpsActor::stateAcquiring);
                break;

            default:
                break;
        }
    }

    // --- stateAcquiring: actively reading GPS ----------------------------

    void stateAcquiring(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _hadFix = false;
                startRepeatingTimer(SIG_READ_GPS, GPS_READ_INTERVAL_MS);
                startTimer(SIG_GPS_TIMEOUT, GPS_TIMEOUT_MS);
                break;

            case SIG_READ_GPS: {
                // Read available data from GPS module in bounded chunks.
                // Yield between chunks so same-priority tasks aren't starved
                // (configUSE_TIME_SLICING = 0 on nRF52).
                char c;
                for (uint8_t chunk = 0; chunk < 5; chunk++) {
                    for (uint8_t i = 0; i < 32; i++) {
                        c = _gps.read();
                        if (c == 0) goto read_done;  // No more data
                    }
                    taskYIELD();  // Let same-priority tasks run
                }
                read_done:

                // Check if we got a new NMEA sentence
                if (_gps.newNMEAreceived()) {
                    _gps.parse(_gps.lastNMEA());
                }

                // Publish satellite count
                {
                    Event satEvt = Event::make(SIG_GPS_SATS);
                    uint8_t sats = _gps.satellites;
                    satEvt.setPayload(sats);
                    publishEvent(satEvt);
                }

                // If we have a valid fix, publish it
                if (_gps.fix) {
                    GpsData data;
                    data.latitude   = _gps.latitudeDegrees;
                    data.longitude  = _gps.longitudeDegrees;
                    data.satellites = _gps.satellites;

                    Event fixEvt = Event::make(SIG_GPS_FIX);
                    fixEvt.setPayload(data);
                    publishEvent(fixEvt);

                    if (!_hadFix) {
                        // First fix (or re-acquired) — cancel timeout
                        cancelTimer(SIG_GPS_TIMEOUT);
                        _hadFix = true;
                    }
                } else if (_hadFix) {
                    // Lost fix — re-arm timeout so SIG_GPS_NO_FIX fires
                    _hadFix = false;
                    startTimer(SIG_GPS_TIMEOUT, GPS_TIMEOUT_MS);
                }
                break;
            }

            case SIG_GPS_TIMEOUT:
                // No fix obtained within timeout
                publishEvent(Event::make(SIG_GPS_NO_FIX));
                break;

            case SIG_GPS_STOP:
                transitionTo(&GpsActor::stateIdle);
                break;

            case SIGNAL_EXIT:
                cancelTimer(SIG_READ_GPS);
                cancelTimer(SIG_GPS_TIMEOUT);
                break;

            default:
                break;
        }
    }
};

#endif // GEOCACHE_GPS_ACTOR_H
