// ---------------------------------------------------------------------------
// SensorDisplay — IMU Actor Publishing to a Display Actor (CLUE-specific)
// ---------------------------------------------------------------------------
// Demonstrates a producer/consumer pattern with two Actors:
//   - SensorActor: reads the LSM6DS33 IMU at a regular interval and
//     publishes acceleration data via the Bus
//   - DisplayActor: subscribes to sensor data and renders it on the
//     CLUE's 240x240 TFT display
//
// This example uses the Adafruit CLUE's onboard IMU (LSM6DS33) and
// TFT display (ST7789). Both libraries must be installed.
//
// Target: Adafruit CLUE (nRF52840)
// ---------------------------------------------------------------------------

#include <Loom.h>
#include <Adafruit_LSM6DS33.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

enum Signal : uint16_t {
    // Framework reserved
    SIG_ENTRY = SIGNAL_ENTRY,
    SIG_EXIT  = SIGNAL_EXIT,

    // Application signals
    SIG_READ_SENSOR = SIGNAL_USER,
    SIG_ACCEL_DATA,
};

// ---------------------------------------------------------------------------
// Acceleration payload — fits within Event's 32-byte payload
// ---------------------------------------------------------------------------

struct AccelData {
    float x;
    float y;
    float z;
};
// sizeof(AccelData) = 12 bytes — well within the 32-byte payload limit

// ---------------------------------------------------------------------------
// SensorActor — periodically reads IMU and publishes acceleration data
// ---------------------------------------------------------------------------

class SensorActor : public Actor<8, SensorActor, 1024> {
public:
    SensorActor() : Actor("Sensor") {}

    void begin() override {
        if (!_imu.begin_I2C()) {
            LOOM_WARN("LSM6DS33 init failed!");
        }
        transitionTo(&SensorActor::stateRunning);
    }

private:
    Adafruit_LSM6DS33 _imu;

    void stateRunning(Event const& event) {
        switch (event.signal) {
            case SIG_ENTRY:
                startRepeatingTimer(SIG_READ_SENSOR, 100);  // 10 Hz
                break;

            case SIG_READ_SENSOR: {
                sensors_event_t accel, gyro, temp;
                _imu.getEvent(&accel, &gyro, &temp);

                AccelData data;
                data.x = accel.acceleration.x;
                data.y = accel.acceleration.y;
                data.z = accel.acceleration.z;

                Event evt = Event::make(SIG_ACCEL_DATA);
                evt.setPayload(data);
                publishEvent(evt);
                break;
            }

            case SIG_EXIT:
                cancelTimer(SIG_READ_SENSOR);
                break;

            default:
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// DisplayActor — renders acceleration data on the CLUE TFT
// ---------------------------------------------------------------------------
// Uses a larger stack (2048 bytes) because display rendering and SPI
// transactions require more stack depth than typical Actors.
// ---------------------------------------------------------------------------

class DisplayActor : public Actor<8, DisplayActor, 2048> {
public:
    DisplayActor() : Actor("Display") {}

    void begin() override {
        // CLUE TFT: ST7789 240x240, CS=32, DC=33, RST=34
        _tft.init(240, 240);
        _tft.setRotation(1);
        _tft.fillScreen(ST77XX_BLACK);
        _tft.setTextSize(2);
        _tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

        _tft.setCursor(10, 10);
        _tft.print("Loom Sensor Demo");

        transitionTo(&DisplayActor::stateRunning);
    }

private:
    // CLUE TFT pin assignments
    static constexpr int TFT_CS  = 32;
    static constexpr int TFT_DC  = 33;
    static constexpr int TFT_RST = 34;

    Adafruit_ST7789 _tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

    void stateRunning(Event const& event) {
        switch (event.signal) {
            case SIG_ACCEL_DATA: {
                AccelData data = event.getPayload<AccelData>();

                // Clear the data area
                _tft.fillRect(10, 50, 220, 80, ST77XX_BLACK);

                _tft.setCursor(10, 50);
                _tft.print("X: "); _tft.println(data.x, 2);

                _tft.setCursor(10, 75);
                _tft.print("Y: "); _tft.println(data.y, 2);

                _tft.setCursor(10, 100);
                _tft.print("Z: "); _tft.println(data.z, 2);
                break;
            }

            default:
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// Sketch
// ---------------------------------------------------------------------------

SensorActor  sensor;
DisplayActor display;

void setup() {
    Serial.begin(115200);

    Loom.registerActor(sensor,  /*priority=*/2);
    Loom.registerActor(display, /*priority=*/1);

    // DisplayActor subscribes to acceleration data from SensorActor
    Loom.subscribe(SIG_ACCEL_DATA, display);

    Loom.begin();
}

void loop() {
    // Empty — all work happens in Actor tasks
}
