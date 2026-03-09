// ---------------------------------------------------------------------------
// ReverseGeocache — Main Sketch
// ---------------------------------------------------------------------------
// A Reverse Geocache Box built on the Loom actor framework for the
// Adafruit CLUE (nRF52840).
//
// The box is locked with a servo and only unlocks when the user visits
// a sequence of GPS waypoints. Progress is persisted to flash so it
// survives power cycles.
//
// Hardware:
//   - Adafruit CLUE (nRF52840) — built-in TFT, buttons, buzzer
//   - Adafruit Mini GPS PA1010D — I2C via STEMMA QT
//   - Micro servo — GPIO pin 2 (via MOSFET for power gating optional)
//   - LiPo battery via CLUE's JST connector
//
// Required libraries:
//   - Loom (this library)
//   - Adafruit GPS Library
//   - Adafruit ST7735 and ST7789 Library
//   - Adafruit GFX Library
//   - Servo
//   - Adafruit SPIFlash
//   - SdFat - Adafruit Fork
//   - ArduinoJson
//   - Adafruit TinyUSB Library
//
// Buttons:
//   - A (left, pin 5):  short press = action, long hold (2s) = arm/reset
//   - B (right, pin 11): short press = secondary action (future use)
//
// Waypoint loading:
//   Waypoints are loaded from waypoints.json on the QSPI flash.
//   The CLUE's 2 MB QSPI flash is exposed as a USB mass storage drive —
//   connect via USB and drag-and-drop waypoints.json onto the drive.
//   See waypoints.example.json for the expected format.
// ---------------------------------------------------------------------------

#include <Loom.h>
#include <Wire.h>

#include "Signals.h"
#include "Waypoints.h"
#include "InputActor.h"
#include "GpsActor.h"
#include "NavActor.h"
#include "BoxActor.h"
#include "DisplayActor.h"

// ---------------------------------------------------------------------------
// Actor instances
// ---------------------------------------------------------------------------

InputActor   inputActor;
GpsActor     gpsActor;
NavActor     navActor;
BoxActor     boxActor;
DisplayActor displayActor;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    // Initialize QSPI flash and USB MSC BEFORE Serial.begin() so that
    // the initial USB enumeration includes both CDC + MSC — no re-enumeration needed.
    initFlashMSC();

    Serial.begin(115200);

    // Initialize I2C for GPS module
    Wire.begin();

    // Wait for USB serial connection (up to 3 seconds)
    uint32_t serialWait = millis();
    while (!Serial && (millis() - serialWait < 3000)) {
        delay(10);
    }

    Serial.println("ReverseGeocache starting...");

    // Mount FAT filesystem on QSPI flash
    if (!mountFATFilesystem()) {
        Serial.println("FAT mount failed — flash may need formatting");
        Serial.println("The drive should appear on your computer");
        while (1) { delay(100); yield(); }
    }

    // Load waypoints from waypoints.json on the FAT filesystem
    bool wpLoaded = WaypointStore::loadFromFile();

    if (!wpLoaded) {
        Serial.println("No waypoints loaded — connect USB and drop waypoints.json");
        Serial.println("Then reset the board");
        while (1) { delay(100); yield(); }
    }

    Serial.println("Setup complete — starting actors...");

    // Register actors with priorities:
    //   3 = InputActor   (highest — responsive button handling)
    //   2 = GpsActor     (time-sensitive GPS reads)
    //   2 = NavActor     (nav computation)
    //   2 = BoxActor     (coordination / servo / buzzer)
    //   1 = DisplayActor (lowest — rendering is non-critical)
    Loom.registerActor(inputActor,   /*priority=*/3);
    Loom.registerActor(gpsActor,     /*priority=*/2);
    Loom.registerActor(navActor,     /*priority=*/2);
    Loom.registerActor(boxActor,     /*priority=*/2);
    Loom.registerActor(displayActor, /*priority=*/1);

    // --- Bus subscriptions -----------------------------------------------

    // InputActor button events → BoxActor (for state transitions)
    Loom.subscribe(SIG_BUTTON_A_PRESS, boxActor);
    Loom.subscribe(SIG_BUTTON_A_HOLD,  boxActor);

    // InputActor button events → DisplayActor (for potential direct handling)
    // (Not strictly needed since DisplayActor follows BoxActor's state,
    //  but included for future extensibility)

    // GpsActor fix/sat events → NavActor (for navigation computation)
    Loom.subscribe(SIG_GPS_FIX,  navActor);

    // GpsActor events → BoxActor (for state transitions)
    Loom.subscribe(SIG_GPS_FIX,    boxActor);
    Loom.subscribe(SIG_GPS_NO_FIX, boxActor);

    // GpsActor satellite count → DisplayActor (for satellite display)
    Loom.subscribe(SIG_GPS_SATS, displayActor);

    // NavActor events → BoxActor (for arrival detection)
    Loom.subscribe(SIG_NAV_ARRIVED, boxActor);

    // NavActor events → DisplayActor (for distance/bearing display)
    Loom.subscribe(SIG_NAV_UPDATE, displayActor);

    // BoxActor state changes → DisplayActor (drives UI state machine)
    Loom.subscribe(SIG_BOX_STATE,     displayActor);
    Loom.subscribe(SIG_WAYPOINT_INFO, displayActor);

    // --- Start the framework ---------------------------------------------

    Loom.begin();
}

// ---------------------------------------------------------------------------
// Loop — empty, all work in Actor tasks
// ---------------------------------------------------------------------------

void loop() {
    // Empty — all work happens in Actor tasks via FreeRTOS
}
