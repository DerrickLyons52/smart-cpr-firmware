
#include <Arduino.h>
#include <Wire.h>
#include <vl53l4cd_class.h>

#define DEV_I2C Wire
#define SerialPort Serial

VL53L4CD sensor(&DEV_I2C, 0);

// Sensor reads about +15 mm high in your setup
const int DISTANCE_OFFSET_MM = 15;

// Match 30 fps video as closely as possible
const unsigned long LOG_INTERVAL_MS = 33;

// Logging state
unsigned long startTimeMs = 0;
unsigned long lastLogTimeMs = 0;
unsigned long sampleIndex = 0;

int applyOffset(int rawMm) {
    int corrected = rawMm - DISTANCE_OFFSET_MM;
    if (corrected < 0) corrected = 0;
    return corrected;
}

void printCsvHeader() {
    SerialPort.println("sample_idx,time_ms,raw_distance_mm,corrected_distance_mm,range_status,signal_per_spad_kcps");
}

void setup() {
    SerialPort.begin(115200);
    delay(1000);

    DEV_I2C.begin();
    sensor.begin();

    if (sensor.InitSensor() != 0) {
        SerialPort.println("InitSensor failed");
        while (1);
    }

    // 33 ms timing budget to align with ~30 fps video
    sensor.VL53L4CD_SetRangeTiming(33, 0);
    sensor.VL53L4CD_StartRanging();

    startTimeMs = millis();
    lastLogTimeMs = startTimeMs;

    printCsvHeader();
}

void loop() {
    unsigned long nowMs = millis();

    // Log at approximately 30 Hz
    if (nowMs - lastLogTimeMs >= LOG_INTERVAL_MS) {
        lastLogTimeMs += LOG_INTERVAL_MS;

        uint8_t newDataReady = 0;
        uint8_t status = 0;
        VL53L4CD_Result_t results;

        // Check if a new measurement is ready
        status = sensor.VL53L4CD_CheckForDataReady(&newDataReady);

        if ((status == 0) && (newDataReady != 0)) {
            sensor.VL53L4CD_ClearInterrupt();
            sensor.VL53L4CD_GetResult(&results);

            int rawDistance = results.distance_mm;
            int correctedDistance = applyOffset(rawDistance);
            unsigned long elapsedMs = nowMs - startTimeMs;

            // CSV row
            SerialPort.print(sampleIndex);
            SerialPort.print(",");
            SerialPort.print(elapsedMs);
            SerialPort.print(",");
            SerialPort.print(rawDistance);
            SerialPort.print(",");
            SerialPort.print(correctedDistance);
            SerialPort.print(",");
            SerialPort.print(results.range_status);
            SerialPort.print(",");
            SerialPort.println(results.signal_per_spad_kcps);

            sampleIndex++;
        }
    }
}