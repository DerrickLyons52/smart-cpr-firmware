#include <Arduino.h>
#include <HX711.h>
#include <EEPROM.h>

const int HX711_DT = 22;
const int HX711_SCK = 23;

HX711 scale;
long tare_offset = 0;

const float KNOWN_MASS_KG = 6.80f;
const float KNOWN_FORCE_N = KNOWN_MASS_KG * 9.81f;

const float DEFAULT_SLOPE_N_PER_COUNT = 0.0010f;
const float DEFAULT_OFFSET_N = 0.0f;

const uint32_t CAL_VALID_KEY = 0x1234ABCD;

// change this to control smoothing vs speed
const int RUN_AVG_SAMPLES = 1;   // was effectively slower at 10
const int CAL_AVG_SAMPLES = 10;  // fine to keep higher for calibration

struct CalibrationData {
  float slope_N_per_count;
  float offset_N;
  uint32_t valid_key;
};

CalibrationData cal;

void tare_scale();
long readAverageRaw(int n);
float countsToNewtons(long raw_tared);
void loadCalibration();
void saveCalibration(float slope, float offset);
void resetCalibration();
void printCalibration();
void handleSerialCommands();

void setup() {
  Serial.begin(115200);
  delay(1000);

  scale.begin(HX711_DT, HX711_SCK);

  if (!scale.is_ready()) {
    Serial.println("ERROR: HX711 not ready");
    while (1) delay(1000);
  }

  loadCalibration();

  Serial.println("Warm up for 5 seconds...");
  delay(5000);

  tare_scale();

  Serial.println("Commands:");
  Serial.println("  t = tare now");
  Serial.println("  p = print calibration");
  Serial.println("  z = save calibration using KNOWN_MASS_KG on sensor");
  Serial.println("  r = reset calibration to defaults");
  Serial.println();
  Serial.println("t_ms,raw_avg,raw_tared,force_N");
}

void loop() {
  handleSerialCommands();

  long raw_avg = readAverageRaw(RUN_AVG_SAMPLES);
  long raw_tared = raw_avg - tare_offset;
  float force_N = countsToNewtons(raw_tared);

  Serial.print(millis());
  Serial.print(",");
  Serial.print(raw_avg);
  Serial.print(",");
  Serial.print(raw_tared);
  Serial.print(",");
  Serial.println(force_N, 6);

  delay(50);
}

void tare_scale() {
  long sum = 0;
  const int tare_samples = 20;

  Serial.println("Taring... keep truly unloaded.");

  for (int i = 0; i < tare_samples; i++) {
    while (!scale.is_ready()) delay(1);
    sum += scale.read();
  }

  tare_offset = sum / tare_samples;

  Serial.print("tare_offset = ");
  Serial.println(tare_offset);
}

long readAverageRaw(int n) {
  long sum = 0;
  for (int i = 0; i < n; i++) {
    while (!scale.is_ready()) delay(1);
    sum += scale.read();
  }
  return sum / n;
}

float countsToNewtons(long raw_tared) {
  return cal.slope_N_per_count * raw_tared + cal.offset_N;
}

void loadCalibration() {
  EEPROM.get(0, cal);

  if (cal.valid_key == CAL_VALID_KEY) {
    Serial.println("Calibration loaded from EEPROM.");
  } else {
    Serial.println("No valid calibration found. Using defaults.");
    cal.slope_N_per_count = DEFAULT_SLOPE_N_PER_COUNT;
    cal.offset_N = DEFAULT_OFFSET_N;
    cal.valid_key = CAL_VALID_KEY;
  }

  printCalibration();
}

void saveCalibration(float slope, float offset) {
  cal.slope_N_per_count = slope;
  cal.offset_N = offset;
  cal.valid_key = CAL_VALID_KEY;

  EEPROM.put(0, cal);

  Serial.println("Calibration saved to EEPROM.");
  printCalibration();
}

void resetCalibration() {
  cal.slope_N_per_count = DEFAULT_SLOPE_N_PER_COUNT;
  cal.offset_N = DEFAULT_OFFSET_N;
  cal.valid_key = CAL_VALID_KEY;

  EEPROM.put(0, cal);

  Serial.println("Calibration reset to defaults.");
  printCalibration();
}

void printCalibration() {
  Serial.print("Slope (N/count): ");
  Serial.println(cal.slope_N_per_count, 10);
  Serial.print("Offset (N): ");
  Serial.println(cal.offset_N, 10);
  Serial.print("Known mass used for 'z' command (kg): ");
  Serial.println(KNOWN_MASS_KG, 6);
  Serial.print("Known force used for 'z' command (N): ");
  Serial.println(KNOWN_FORCE_N, 6);
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (cmd == 't' || cmd == 'T') {
    tare_scale();
  }
  else if (cmd == 'p' || cmd == 'P') {
    printCalibration();
  }
  else if (cmd == 'r' || cmd == 'R') {
    resetCalibration();
  }
  else if (cmd == 'z' || cmd == 'Z') {
    Serial.println("Calibrating from current loaded reading...");

    long raw_avg = readAverageRaw(CAL_AVG_SAMPLES);
    long raw_tared = raw_avg - tare_offset;

    if (raw_tared == 0) {
      Serial.println("ERROR: raw_tared is zero. Cannot calibrate.");
      return;
    }

    float new_slope = KNOWN_FORCE_N / (float)raw_tared;
    float new_offset = 0.0f;

    saveCalibration(new_slope, new_offset);

    Serial.println("Calibration complete.");
    Serial.println("Make sure the known weight was applied steadily and correctly.");
  }
}