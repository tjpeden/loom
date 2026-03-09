// ---------------------------------------------------------------------------
// ReverseGeocache — Waypoint Store Implementation
// ---------------------------------------------------------------------------
// Initializes QSPI flash, mounts FAT filesystem, exposes USB mass storage,
// and parses waypoints.json using ArduinoJson.
// ---------------------------------------------------------------------------

#include "Waypoints.h"
#include <Arduino.h>
#include <SPI.h>
#include <SdFat_Adafruit_Fork.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// QSPI Flash + FAT + USB MSC globals
// ---------------------------------------------------------------------------

// QSPI transport — auto-picks CLUE's QSPI pins from variant.h
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// FAT filesystem on the QSPI flash
FatVolume fatfs;

// USB Mass Storage
Adafruit_USBD_MSC usb_msc;

// Set true by MSC flush callback when host writes to the drive
volatile bool fs_changed = false;

// ---------------------------------------------------------------------------
// MSC callbacks — invoked from USB interrupt context
// ---------------------------------------------------------------------------

static int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
    return flash.readBlocks(lba, (uint8_t*)buffer, bufsize / 512) ? (int32_t)bufsize : -1;
}

static int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
    return flash.writeBlocks(lba, buffer, bufsize / 512) ? (int32_t)bufsize : -1;
}

static void msc_flush_cb(void) {
    flash.syncBlocks();
    fatfs.cacheClear();
    fs_changed = true;
}

// ---------------------------------------------------------------------------
// WaypointStore static members
// ---------------------------------------------------------------------------

Waypoint WaypointStore::_waypoints[MAX_WAYPOINTS];
uint8_t  WaypointStore::_count = 0;
double   WaypointStore::_arrivalThreshold = DEFAULT_ARRIVAL_THRESHOLD;
bool     WaypointStore::_loaded = false;

// ---------------------------------------------------------------------------
// initFlashMSC() — call BEFORE Serial.begin()
// ---------------------------------------------------------------------------
// By calling usb_msc.begin() before the USB stack starts (i.e. before
// Serial.begin()), the initial USB enumeration will already include
// both CDC (serial) and MSC (mass storage). No detach/attach needed.
// ---------------------------------------------------------------------------

void initFlashMSC() {
    flash.begin();

    // Mount FAT filesystem BEFORE enabling MSC, so the host OS
    // can't interfere with our read of waypoints.json
    fatfs.begin(&flash);

    // Set up USB Mass Storage
    usb_msc.setID("Geocache", "Waypoints", "1.0");
    usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
    usb_msc.setCapacity(flash.size() / 512, 512);
    usb_msc.setUnitReady(true);
    usb_msc.begin();
}

// ---------------------------------------------------------------------------
// mountFATFilesystem() — call AFTER Serial.begin()
// ---------------------------------------------------------------------------

bool mountFATFilesystem() {
    if (flash.size() == 0) {
        Serial.println("[WP] QSPI flash not initialized!");
        return false;
    }

    Serial.print("[WP] Flash size: ");
    Serial.print(flash.size() / 1024);
    Serial.println(" KB");

    // FAT was already mounted in initFlashMSC(). Verify it worked
    // by trying to open the root directory.
    FatFile root;
    if (!root.open("/")) {
        Serial.println("[WP] FAT mount failed — flash may need formatting");
        Serial.println("[WP] Use Adafruit_SPIFlash SdFat_format example to format");
        Serial.println("[WP] The drive should appear on your computer for formatting");
        return false;
    }
    root.close();

    Serial.println("[WP] FAT filesystem mounted");
    return true;
}

// ---------------------------------------------------------------------------
// WaypointStore::loadFromFile()
// ---------------------------------------------------------------------------

bool WaypointStore::loadFromFile() {
    static constexpr const char* FILENAME = "waypoints.json";

    FatFile file;
    if (!file.open(FILENAME, O_RDONLY)) {
        Serial.println("[WP] waypoints.json not found on flash drive");
        Serial.println("[WP] Connect via USB and drop waypoints.json on the drive");
        _loaded = false;
        return false;
    }

    // Read entire file into a buffer (max ~2 KB expected)
    uint32_t fileSize = file.fileSize();

    if (fileSize == 0 || fileSize > 4096) {
        Serial.print("[WP] Invalid file size: ");
        Serial.println(fileSize);
        file.close();
        _loaded = false;
        return false;
    }

    static char buf[4096];  // static to avoid stack overflow in setup() task
    int bytesRead = file.read(buf, fileSize);
    file.close();

    if (bytesRead != (int)fileSize) {
        Serial.println("[WP] Failed to read waypoints.json");
        _loaded = false;
        return false;
    }
    buf[bytesRead] = '\0';

    // Disable MSC during parsing to prevent USB interrupt contention
    // with ArduinoJson's heap allocation
    usb_msc.setUnitReady(false);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, buf, bytesRead);

    usb_msc.setUnitReady(true);

    if (err) {
        Serial.print("[WP] JSON parse error: ");
        Serial.println(err.c_str());
        _loaded = false;
        return false;
    }

    // Read arrival threshold (optional, default 0.0075 miles)
    _arrivalThreshold = doc["arrival_threshold"] | DEFAULT_ARRIVAL_THRESHOLD;

    // Read waypoints array
    JsonArray arr = doc["waypoints"].as<JsonArray>();
    if (arr.isNull()) {
        Serial.println("[WP] No 'waypoints' array in JSON");
        _loaded = false;
        return false;
    }

    _count = 0;
    for (JsonObject wp : arr) {
        if (_count >= MAX_WAYPOINTS) {
            Serial.println("[WP] Warning: max 16 waypoints, ignoring extras");
            break;
        }

        Waypoint& w = _waypoints[_count];
        w.latitude  = wp["lat"] | 0.0;
        w.longitude = wp["lon"] | 0.0;

        // Copy name
        const char* name = wp["name"] | "???";
        strncpy(w.name, name, sizeof(w.name) - 1);
        w.name[sizeof(w.name) - 1] = '\0';

        // Copy description
        const char* desc = wp["desc"] | "";
        strncpy(w.description, desc, sizeof(w.description) - 1);
        w.description[sizeof(w.description) - 1] = '\0';

        // Validate coordinates
        if (w.latitude == 0.0 && w.longitude == 0.0) {
            Serial.print("[WP] Warning: waypoint '");
            Serial.print(w.name);
            Serial.println("' has zero coordinates, skipping");
            continue;
        }

        _count++;
    }

    _loaded = (_count > 0);

    Serial.print("[WP] Loaded ");
    Serial.print(_count);
    Serial.print(" waypoints, threshold=");
    Serial.print(_arrivalThreshold, 4);
    Serial.println(" mi");

    return _loaded;
}
