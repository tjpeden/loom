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
#include <SPI.h>

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

static constexpr Signal SIG_READ_SENSOR = SIGNAL_USER;
static constexpr Signal SIG_ACCEL_DATA  = SIGNAL_USER + 1;

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
            case SIGNAL_ENTRY:
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

            case SIGNAL_EXIT:
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
        // CLUE TFT backlight — must be turned on
        pinMode(TFT_LITE, OUTPUT);
        digitalWrite(TFT_LITE, HIGH);

        // CLUE TFT: ST7789 240x240 on SPI1
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
    // CLUE TFT pin assignments (from variant.h)
    static constexpr int TFT_CS   = 31;
    static constexpr int TFT_DC   = 32;
    static constexpr int TFT_RST  = 33;
    static constexpr int TFT_LITE = 34;

    // CLUE TFT is on SPI1 (SPIM3, the high-speed 32 MHz bus)
    Adafruit_ST7789 _tft = Adafruit_ST7789(&SPI1, TFT_CS, TFT_DC, TFT_RST);

    void stateRunning(Event const& event) {
        switch (event.signal) {
            case SIG_ACCEL_DATA: {
                AccelData data = event.getPayload<AccelData>();

                // Overwrite previous text in place — setTextColor(fg, bg)
                // fills the background behind each character, eliminating
                // the need for fillRect() and the flicker it causes.

                _tft.setCursor(10, 50);
                _tft.print("X: "); printPadded(data.x);

                _tft.setCursor(10, 75);
                _tft.print("Y: "); printPadded(data.y);

                _tft.setCursor(10, 100);
                _tft.print("Z: "); printPadded(data.z);
                break;
            }

            default:
                break;
        }
    }

    /// Print a float right-padded with spaces so shorter values overwrite
    /// any leftover characters from the previous longer value.
    void printPadded(float value) {
        int n = _tft.print(value, 2);
        // Pad to a fixed width (8 chars covers -XX.XX plus margin)
        for (int i = n; i < 8; i++) {
            _tft.print(' ');
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
