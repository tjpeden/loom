// ---------------------------------------------------------------------------
// ReverseGeocache — DisplayActor
// ---------------------------------------------------------------------------
// Renders UI on the CLUE's 240x240 ST7789 TFT display. Subscribes to
// box state changes, navigation updates, waypoint info, and satellite
// counts. Each box state maps to a display state with its own rendering.
//
// Uses setTextColor(fg, bg) to avoid flicker from fillRect().
// ---------------------------------------------------------------------------

#ifndef GEOCACHE_DISPLAY_ACTOR_H
#define GEOCACHE_DISPLAY_ACTOR_H

#include "Signals.h"
#include "NavActor.h"  // For bearingToCardinal()
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include <SPI.h>

class DisplayActor : public Actor<8, DisplayActor, 2048> {
public:
    DisplayActor() : Actor("Display") {}

    void begin() override {
        // Turn on CLUE TFT backlight
        pinMode(TFT_LITE, OUTPUT);
        digitalWrite(TFT_LITE, HIGH);

        // Initialize TFT
        _tft.init(240, 240);
        _tft.setRotation(1);
        _tft.fillScreen(COLOR_BG);

        transitionTo(&DisplayActor::stateInit);
    }

private:
    // CLUE TFT pins
    static constexpr int TFT_CS   = 31;
    static constexpr int TFT_DC   = 32;
    static constexpr int TFT_RST  = 33;
    static constexpr int TFT_LITE = 34;

    // CLUE TFT on SPI1 (SPIM3)
    Adafruit_ST7789 _tft = Adafruit_ST7789(&SPI1, TFT_CS, TFT_DC, TFT_RST);

    // Color palette
    static constexpr uint16_t COLOR_BG      = 0x0000;  // Black
    static constexpr uint16_t COLOR_TEXT     = 0xFFFF;  // White
    static constexpr uint16_t COLOR_TITLE    = 0x07FF;  // Cyan
    static constexpr uint16_t COLOR_ACCENT   = 0x07E0;  // Green
    static constexpr uint16_t COLOR_WARN     = 0xFBE0;  // Yellow
    static constexpr uint16_t COLOR_ERROR    = 0xF800;  // Red
    static constexpr uint16_t COLOR_DIM      = 0x7BEF;  // Gray
    static constexpr uint16_t COLOR_ARMED    = 0x001F;  // Blue

    // Cached data for refreshing
    NavData      _lastNav     = {};
    uint8_t      _lastSats    = 0;
    WaypointInfo _lastWpInfo  = {};

    // --- Screen layout helpers -------------------------------------------

    void drawTitle(const char* title, uint16_t borderColor = COLOR_TITLE) {
        _tft.fillScreen(COLOR_BG);
        _tft.drawRoundRect(2, 2, 236, 24, 4, borderColor);
        _tft.setTextSize(2);
        _tft.setTextColor(COLOR_TEXT, COLOR_BG);
        _tft.setCursor(8, 6);
        _tft.print(title);
    }

    void printPadded(const char* text, int padTo = 20) {
        int n = _tft.print(text);
        for (int i = n; i < padTo; i++) _tft.print(' ');
    }

    void printPaddedFloat(float value, int decimals = 2, int padTo = 10) {
        int n = _tft.print(value, decimals);
        for (int i = n; i < padTo; i++) _tft.print(' ');
    }

    void printPaddedInt(int value, int padTo = 6) {
        int n = _tft.print(value);
        for (int i = n; i < padTo; i++) _tft.print(' ');
    }

    // --- State: Init (splash screen) -------------------------------------

    void stateInit(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                drawTitle("Geocache Box");
                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_ACCENT, COLOR_BG);
                _tft.setCursor(20, 80);
                _tft.print("Reverse");
                _tft.setCursor(20, 105);
                _tft.print("Geocache Box");
                _tft.setTextColor(COLOR_DIM, COLOR_BG);
                _tft.setCursor(20, 150);
                _tft.setTextSize(1);
                _tft.print("Powered by Loom v0.2.1");
                break;

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: Unarmed --------------------------------------------------

    void stateUnarmed(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                drawTitle("Unarmed", COLOR_WARN);
                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_WARN, COLOR_BG);
                _tft.setCursor(10, 80);
                _tft.print("Box is unlocked");

                _tft.setTextColor(COLOR_DIM, COLOR_BG);
                _tft.setTextSize(1);
                _tft.setCursor(10, 200);
                _tft.print("Hold A to arm & lock");
                break;

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: Arming ---------------------------------------------------

    void stateArming(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                drawTitle("Arming", COLOR_ARMED);
                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_ACCENT, COLOR_BG);
                _tft.setCursor(10, 80);
                _tft.print("Locking box...");
                break;

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: Acquiring GPS fix ----------------------------------------

    void stateAcquiring(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                _lastSats = 0;
                drawTitle("Acquiring Fix", COLOR_ARMED);
                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_WARN, COLOR_BG);
                _tft.setCursor(10, 70);
                _tft.print("Please wait...");

                _tft.setTextColor(COLOR_TEXT, COLOR_BG);
                _tft.setCursor(10, 120);
                _tft.print("Sats: ");
                printPaddedInt(0);
                break;

            case SIG_GPS_SATS: {
                uint8_t sats = event.getPayload<uint8_t>();
                if (sats != _lastSats) {
                    _lastSats = sats;
                    _tft.setTextSize(2);
                    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
                    _tft.setCursor(82, 120);
                    printPaddedInt(sats);
                }
                break;
            }

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: Navigating -----------------------------------------------

    void stateNavigating(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                drawTitle("Navigation", COLOR_ARMED);

                // Static labels
                _tft.setTextSize(1);
                _tft.setTextColor(COLOR_DIM, COLOR_BG);
                _tft.setCursor(10, 35);
                _tft.print("DISTANCE");
                _tft.setCursor(10, 105);
                _tft.print("DIRECTION");
                _tft.setCursor(10, 175);
                _tft.print("SATELLITES");
                break;

            case SIG_NAV_UPDATE: {
                NavData nav = event.getPayload<NavData>();
                _lastNav = nav;

                // Distance
                _tft.setTextSize(3);
                _tft.setTextColor(COLOR_ACCENT, COLOR_BG);
                _tft.setCursor(10, 50);

                if (nav.distanceMiles < 0.1f) {
                    float ft = nav.distanceMiles * 5280.0f;
                    printPaddedFloat(ft, 0, 8);
                    _tft.print(" ft  ");
                } else {
                    printPaddedFloat(nav.distanceMiles, 2, 8);
                    _tft.print(" mi  ");
                }

                // Bearing / Cardinal direction
                _tft.setCursor(10, 120);
                const char* cardinal = NavActor::bearingToCardinal(nav.bearing);
                printPadded(cardinal, 6);

                // Waypoint progress
                _tft.setTextSize(1);
                _tft.setTextColor(COLOR_DIM, COLOR_BG);
                _tft.setCursor(140, 35);
                char wpStr[16];
                snprintf(wpStr, sizeof(wpStr), "WP %u/%u", nav.wpIndex + 1, nav.wpTotal);
                printPadded(wpStr, 10);
                break;
            }

            case SIG_GPS_SATS: {
                uint8_t sats = event.getPayload<uint8_t>();
                _lastSats = sats;
                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_TEXT, COLOR_BG);
                _tft.setCursor(10, 190);
                printPaddedInt(sats);
                break;
            }

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: Checkpoint -----------------------------------------------

    void stateCheckpoint(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                drawTitle("Checkpoint!", COLOR_ACCENT);
                break;

            case SIG_WAYPOINT_INFO: {
                WaypointInfo info = event.getPayload<WaypointInfo>();
                _lastWpInfo = info;

                // Waypoint name
                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_ACCENT, COLOR_BG);
                _tft.setCursor(10, 50);
                printPadded(info.name, 16);

                // Description
                _tft.setTextSize(1);
                _tft.setTextColor(COLOR_TEXT, COLOR_BG);
                _tft.setCursor(10, 85);
                _tft.print(info.desc);

                // Instructions
                _tft.setTextColor(COLOR_WARN, COLOR_BG);
                _tft.setCursor(10, 200);
                if (info.index + 1 >= info.total) {
                    _tft.print("Press A to open box!");
                } else {
                    _tft.print("Press A to continue");
                }

                // Progress
                _tft.setTextColor(COLOR_DIM, COLOR_BG);
                _tft.setCursor(10, 220);
                char progStr[24];
                snprintf(progStr, sizeof(progStr), "Waypoint %u of %u",
                         info.index + 1, info.total);
                _tft.print(progStr);
                break;
            }

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: OpenBox --------------------------------------------------

    void stateOpenBox(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                drawTitle("Open the Box!", COLOR_ACCENT);
                _tft.setTextSize(3);
                _tft.setTextColor(COLOR_ACCENT, COLOR_BG);
                _tft.setCursor(10, 60);
                _tft.print("You made");
                _tft.setCursor(10, 90);
                _tft.print("it!");

                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_TEXT, COLOR_BG);
                _tft.setCursor(10, 140);
                _tft.print("Open the box.");

                _tft.setTextSize(1);
                _tft.setTextColor(COLOR_DIM, COLOR_BG);
                _tft.setCursor(10, 220);
                _tft.print("Hold A to reset");
                break;

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: GPS failure ----------------------------------------------

    void stateGpsFail(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                drawTitle("GPS Failure", COLOR_ERROR);
                _tft.setTextSize(2);
                _tft.setTextColor(COLOR_ERROR, COLOR_BG);
                _tft.setCursor(10, 70);
                _tft.print("No GPS signal!");
                _tft.setTextSize(1);
                _tft.setTextColor(COLOR_WARN, COLOR_BG);
                _tft.setCursor(10, 120);
                _tft.print("Ensure clear sky view");
                _tft.setTextColor(COLOR_DIM, COLOR_BG);
                _tft.setCursor(10, 200);
                _tft.print("Press A to retry");
                break;

            case SIG_BOX_STATE:
                _handleStateTransition(event);
                break;

            default:
                break;
        }
    }

    // --- State: Sleeping -------------------------------------------------

    void stateSleeping(Event const& event) {
        switch (event.signal) {
            case SIGNAL_ENTRY:
                // Turn off backlight to save power
                digitalWrite(TFT_LITE, LOW);
                break;

            case SIG_BOX_STATE: {
                // Wake up — turn backlight back on
                digitalWrite(TFT_LITE, HIGH);
                _handleStateTransition(event);
                break;
            }

            default:
                break;
        }
    }

    // --- State transition router -----------------------------------------

    void _handleStateTransition(Event const& event) {
        BoxStatePayload bs = event.getPayload<BoxStatePayload>();
        switch (bs.state) {
            case BoxState::Loading:
                // Stay in current state — loading is brief
                break;
            case BoxState::Unarmed:
                transitionTo(&DisplayActor::stateUnarmed);
                break;
            case BoxState::Arming:
                transitionTo(&DisplayActor::stateArming);
                break;
            case BoxState::Acquiring:
                transitionTo(&DisplayActor::stateAcquiring);
                break;
            case BoxState::Navigating:
                transitionTo(&DisplayActor::stateNavigating);
                break;
            case BoxState::Checkpoint:
                transitionTo(&DisplayActor::stateCheckpoint);
                break;
            case BoxState::OpenBox:
                transitionTo(&DisplayActor::stateOpenBox);
                break;
            case BoxState::GpsFail:
                transitionTo(&DisplayActor::stateGpsFail);
                break;
            case BoxState::Sleeping:
                transitionTo(&DisplayActor::stateSleeping);
                break;
        }
    }
};

#endif // GEOCACHE_DISPLAY_ACTOR_H
