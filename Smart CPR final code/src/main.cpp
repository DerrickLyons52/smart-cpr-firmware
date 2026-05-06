
#include <Arduino.h>

#include "tof.h"
#include "compression_analysis.h"
#include "pump_control.h"
#include "hand_placement.h"
#include <HX711.h>
#include <EEPROM.h>
#include <math.h>

// ---------------- Depth / motor ----------------
static const float GOOD_DEPTH_MM = 50.8f;   // 2 inches
static const float MAX_LEAN_FOR_FLOW_MM = 25.0f;
static const unsigned long PULSE_DURATION_MS = 250;

static bool motorPulseActive = false;
static unsigned long pulseStartMs = 0;

// ---------------- Session / serial streaming ----------------
static bool sessionActive = false;
static unsigned long sessionStartMs = 0;
static unsigned long sessionDurationMs = 0;

static const unsigned long LOG_INTERVAL_MS = 33; // ~30 Hz
static unsigned long lastLogMs = 0;

// ---------------- End-of-session result motor ----------------
static const unsigned long RESULT_MOTOR_DURATION_MS = 15000;

static bool resultMotorActive = false;
static unsigned long resultMotorStartMs = 0;

// ---------------- FSR setup ----------------
static const int NUM_SENSORS = 8;

static const int fsrPins[NUM_SENSORS] = {
  A8, A9, A10, A11, A12, A13, A14, A15
};

static float fsrBaseline[NUM_SENSORS];

static const int FSR_BASELINE_SAMPLES = 150;
static const float FSR_SENSOR_DEADBAND = 5.0f;

// ---------------- Hand placement tuning ----------------
static const float HP_ALPHA = 0.25f;

static const float HP_TAU_ENTER_MM = 8.0f;
static const float HP_TAU_EXIT_MM  = 5.0f;

static const float HP_OK_RADIUS_MM       = 8.0f;
static const float HP_MILD_RADIUS_MM     = 15.0f;
static const float HP_MODERATE_RADIUS_MM = 25.0f;

static HandPlacementState hpState;

// ---------------- Load cell ----------------
const int HX711_DT = 22;
const int HX711_SCK = 23;

HX711 scale;
long tare_offset = 0;

const float DEFAULT_SLOPE_N_PER_COUNT = 0.0010f;
const float DEFAULT_OFFSET_N = 0.0f;
const uint32_t CAL_VALID_KEY = 0x1234ABCD;
const float LOADCELL_BASELINE_ALPHA = 0.001f;
const float LOADCELL_ZERO_WINDOW_N = 5.0f;

const int LOAD_CELL_AVG_SAMPLES = 1;

struct CalibrationData {
  float slope_N_per_count;
  float offset_N;
  uint32_t valid_key;
};

CalibrationData cal;

// ---------------- Helpers ----------------
long readLoadCellAverageRaw(int n) {
  long sum = 0;
  for (int i = 0; i < n; i++) {
    while (!scale.is_ready()) delay(1);
    sum += scale.read();
  }
  return sum / n;
}

void tareLoadCell() {
  long sum = 0;
  const int tare_samples = 20;

  Serial.println("Taring load cell... keep unloaded.");

  for (int i = 0; i < tare_samples; i++) {
    while (!scale.is_ready()) delay(1);
    sum += scale.read();
  }

  tare_offset = sum / tare_samples;

  Serial.print("Load cell tare_offset = ");
  Serial.println(tare_offset);
}

void loadLoadCellCalibration() {
  EEPROM.get(0, cal);

  if (cal.valid_key == CAL_VALID_KEY) {
    Serial.println("Load cell calibration loaded.");
  } else {
    Serial.println("No valid load cell calibration. Using defaults.");
    cal.slope_N_per_count = DEFAULT_SLOPE_N_PER_COUNT;
    cal.offset_N = DEFAULT_OFFSET_N;
    cal.valid_key = CAL_VALID_KEY;
  }
}

float countsToNewtons(long raw_tared) {
  return cal.slope_N_per_count * raw_tared + cal.offset_N;
}

float readLoadCellForceN(bool allowFloatingBaseline) {
  long raw_avg = readLoadCellAverageRaw(LOAD_CELL_AVG_SAMPLES);
  long raw_tared = raw_avg - tare_offset;
  float force_N = countsToNewtons(raw_tared);

  if (allowFloatingBaseline && fabs(force_N) < LOADCELL_ZERO_WINDOW_N) {
    tare_offset = (long)((1.0f - LOADCELL_BASELINE_ALPHA) * tare_offset
              + LOADCELL_BASELINE_ALPHA * raw_avg);
  }

  return force_N;
}

void triggerMotorPulse() {
  motorPulseActive = true;
  pulseStartMs = millis();
  pump_set_binary(true);
}

void updateMotorPulse() {
  if (motorPulseActive && (millis() - pulseStartMs >= PULSE_DURATION_MS)) {
    motorPulseActive = false;
    pump_set_binary(false);
  }
}

void triggerResultMotor() {
  resultMotorActive = true;
  resultMotorStartMs = millis();

  motorPulseActive = false;   // do not mix with pulse logic
  pump_set_binary(true);
}

void updateResultMotor() {
  if (resultMotorActive && (millis() - resultMotorStartMs >= RESULT_MOTOR_DURATION_MS)) {
    resultMotorActive = false;
    pump_set_binary(false);

    Serial.println("RESULT_MOTOR_COMPLETE");
  }
}

void calibrateFsrBaseline() {
  Serial.println("Hold still for FSR tare...");

  for (int i = 0; i < NUM_SENSORS; i++) {
    fsrBaseline[i] = 0.0f;
  }

  for (int n = 0; n < FSR_BASELINE_SAMPLES; n++) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      fsrBaseline[i] += analogRead(fsrPins[i]);
    }
    delay(10);
  }

  for (int i = 0; i < NUM_SENSORS; i++) {
    fsrBaseline[i] /= FSR_BASELINE_SAMPLES;
  }

  Serial.println("FSR tare complete.");
}

void readFsrActivities(float s[NUM_SENSORS]) {
  for (int i = 0; i < NUM_SENSORS; i++) {
    float raw = analogRead(fsrPins[i]);
    float act = raw - fsrBaseline[i];

    if (act < 0.0f) act = 0.0f;
    if (act < FSR_SENSOR_DEADBAND) act = 0.0f;

    s[i] = act;
  }
}

void printCsvHeader() {
  Serial.println("time_ms,session_time_ms,raw_depth_mm,filtered_depth_mm,peak_depth_mm,lean_mm,rate_cpm,compression_started,compression_completed,motion_state,hp_x,hp_y,hp_s,force_N,motor_on");
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("START")) {
    int commaIndex = cmd.indexOf(',');

    if (commaIndex > 0) {
      int seconds = cmd.substring(commaIndex + 1).toInt();

      sessionActive = true;
      sessionStartMs = millis();
      sessionDurationMs = (unsigned long)seconds * 1000;
      lastLogMs = millis();

      pump_set_binary(false);
      motorPulseActive = false;

      Serial.print("SESSION_STARTED,");
      Serial.println(seconds);

      printCsvHeader();
    }
  }
  else if (cmd == "STOP") {
    sessionActive = false;
    motorPulseActive = false;
    resultMotorActive = false;
    pump_set_binary(false);

    Serial.println("SESSION_STOPPED");
  }
  else if (cmd.startsWith("RESULT")) {
  int commaIndex = cmd.indexOf(',');

  if (commaIndex > 0) {
    int result = cmd.substring(commaIndex + 1).toInt();

    if (result == 1) {
      Serial.println("RESULT_SUCCESS");
      triggerResultMotor();
    } else {
      Serial.println("RESULT_FAIL");
      resultMotorActive = false;
      pump_set_binary(false);
    }
  }
}
}

void checkSessionTimeout() {
  if (sessionActive && (millis() - sessionStartMs >= sessionDurationMs)) {
    sessionActive = false;
    motorPulseActive = false;
    pump_set_binary(false);

    Serial.println("SESSION_COMPLETE");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("=== CPR Feedback System Test ===");

  pump_init();

  scale.begin(HX711_DT, HX711_SCK);

  if (!scale.is_ready()) {
    Serial.println("ERROR: HX711 not ready");
    while (1) {}
  }

  loadLoadCellCalibration();

  Serial.println("Warm up load cell for 5 seconds...");
  delay(5000);

  tareLoadCell();

  if (!tof_init()) {
    Serial.println("ToF init failed.");
    while (1) {}
  }

  Serial.println("Hold still for ToF tare...");
  if (!tof_tareBaseline(2000)) {
    Serial.println("ToF tare failed.");
    while (1) {}
  }

  calibrateFsrBaseline();
  handPlacementInitGeometry();
  compressionAnalysisInit();

  Serial.println("System ready. Start compressions.");
  Serial.println("ARDUINO_READY");
}

void loop() {
  handleSerialCommands();
  ToFReading tf = tof_update();
  CompressionMetrics cm = compressionAnalysisUpdate(tf);

  float fsrActivities[NUM_SENSORS];
  readFsrActivities(fsrActivities);

  HandPlacementResult hp = computeHandPlacement(
    fsrActivities,
    FSR_SENSOR_DEADBAND,
    HP_ALPHA,
    HP_TAU_ENTER_MM,
    HP_TAU_EXIT_MM,
    HP_OK_RADIUS_MM,
    HP_MILD_RADIUS_MM,
    HP_MODERATE_RADIUS_MM,
    &hpState
  );

  bool compressionActive =
  cm.motion_state == CompressionMotionState::COMPRESSING ||
  cm.motion_state == CompressionMotionState::RELEASING;

  float force_N = readLoadCellForceN(!compressionActive);

  updateMotorPulse();
  updateResultMotor();
  checkSessionTimeout();

  if (sessionActive && cm.compression_completed) {

  unsigned long sessionTimeMs = millis() - sessionStartMs;

  // ---- Send completed compression summary to Pi ----
  Serial.print("COMP,");
  Serial.print(sessionTimeMs);
  Serial.print(",");
  Serial.print(cm.peak_depth_mm, 1);
  Serial.print(",");
  Serial.print(cm.lean_mm, 1);
  Serial.print(",");
  Serial.print(cm.rate_cpm, 1);
  Serial.print(",");
  Serial.print(hp.xf, 1);
  Serial.print(",");
  Serial.print(hp.yf, 1);
  Serial.print(",");
  Serial.print(hp.S, 1);
  Serial.print(",");
  Serial.println(force_N, 1);

  // ---- Existing motor pulse logic (unchanged) ----
  if (!resultMotorActive) {
    if (cm.peak_depth_mm >= GOOD_DEPTH_MM && cm.lean_mm <= MAX_LEAN_FOR_FLOW_MM) {
      triggerMotorPulse();
    }
  }
}

  if (sessionActive && (millis() - lastLogMs >= LOG_INTERVAL_MS)) {
    lastLogMs = millis();

    unsigned long now = millis();
    unsigned long sessionTimeMs = now - sessionStartMs;

    Serial.print(now);
    Serial.print(",");
    Serial.print(sessionTimeMs);
    Serial.print(",");
    Serial.print(cm.raw_depth_mm, 1);
    Serial.print(",");
    Serial.print(cm.filtered_depth_mm, 1);
    Serial.print(",");
    Serial.print(cm.peak_depth_mm, 1);
    Serial.print(",");
    Serial.print(cm.lean_mm, 1);
    Serial.print(",");
    Serial.print(cm.rate_cpm, 1);
    Serial.print(",");
    Serial.print(cm.compression_started ? 1 : 0);
    Serial.print(",");
    Serial.print(cm.compression_completed ? 1 : 0);
    Serial.print(",");
    Serial.print((int)cm.motion_state);
    Serial.print(",");
    Serial.print(hp.xf, 1);
    Serial.print(",");
    Serial.print(hp.yf, 1);
    Serial.print(",");
    Serial.print(hp.S, 1);
    Serial.print(",");
    Serial.print(force_N, 1);
    Serial.print(",");
    Serial.println(motorPulseActive || resultMotorActive ? 1 : 0);
  }
}