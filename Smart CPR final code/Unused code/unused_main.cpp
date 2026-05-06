// //////////////////////////////////////////////////////////// motor pulse with good depth ////////////////////////////////////////////////////////////////////////////

// #include <Arduino.h>

// #include "tof.h"
// #include "compression_analysis.h"
// #include "pump_control.h"

// static const float GOOD_DEPTH_MM = 50.8f;          // 2 inches
// static const unsigned long PULSE_DURATION_MS = 250;

// static bool motorPulseActive = false;
// static unsigned long pulseStartMs = 0;

// void triggerMotorPulse() {
//   motorPulseActive = true;
//   pulseStartMs = millis();
//   pump_set_binary(true);
// }

// void updateMotorPulse() {
//   if (motorPulseActive && (millis() - pulseStartMs >= PULSE_DURATION_MS)) {
//     motorPulseActive = false;
//     pump_set_binary(false);
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   while (!Serial) {}

//   Serial.println("=== Motor Pulse Depth Test ===");

//   pump_init();

//   if (!tof_init()) {
//     Serial.println("ToF init failed.");
//     while (1) {}
//   }

//   Serial.println("Hold still for ToF tare...");
//   if (!tof_tareBaseline(2000)) {
//     Serial.println("ToF tare failed.");
//     while (1) {}
//   }

//   compressionAnalysisInit();

//   Serial.println("System ready. Start compressions.");
// }

// void loop() {
//   ToFReading tf = tof_update();
//   CompressionMetrics cm = compressionAnalysisUpdate(tf);

//   updateMotorPulse();

//   if (cm.compression_completed) {
//     Serial.print("Peak=");
//     Serial.print(cm.peak_depth_mm, 1);

//     Serial.print(" mm | Lean=");
//     Serial.print(cm.lean_mm, 1);

//     Serial.print(" mm | Rate=");
//     Serial.print(cm.rate_cpm, 1);

//     if (cm.peak_depth_mm >= GOOD_DEPTH_MM) {
//       Serial.println(" | GOOD DEPTH -> PULSE");
//       triggerMotorPulse();
//     } else {
//       Serial.println(" | TOO SHALLOW");
//     }
//   }
// }


//////////////////////////////////////////////////  HP_x and HP_y and depth reading with python (in PyCharm) ////////////////////////////////////////////////////////////

// #include <Arduino.h>
// #include <math.h>

// #include "tof.h"

// // ---------------- FSR setup ----------------
// static const int NUM_SENSORS = 8;

// static const int fsrPins[NUM_SENSORS] = {
//   A8, A9, A10, A11, A12, A13, A14, A15
// };

// static float fsrBaseline[NUM_SENSORS];
// static const int FSR_BASELINE_SAMPLES = 150;
// static const float FSR_SENSOR_DEADBAND = 5.0f;

// // ---------------- Hand placement geometry ----------------
// static const float r_mm[NUM_SENSORS] = {
//   30.0f, 30.96f, 31.24f, 29.85f, 30.0f, 30.25f, 30.28f, 30.93f
// };

// static const float theta_deg[NUM_SENSORS] = {
//   0.0f, 45.0f, 89.0f, 136.0f, 180.0f, 225.0f, 269.0f, 307.0f
// };

// static float x_pos[NUM_SENSORS];
// static float y_pos[NUM_SENSORS];

// // ---------------- Logging ----------------
// static const unsigned long LOG_INTERVAL_MS = 33; // ~30 Hz
// static unsigned long lastLogMs = 0;

// // ---------------- Helpers ----------------
// static float degToRad(float deg) {
//   return deg * (PI / 180.0f);
// }

// void setupGeometry() {
//   for (int i = 0; i < NUM_SENSORS; i++) {
//     float theta = degToRad(theta_deg[i]);
//     x_pos[i] = r_mm[i] * sinf(theta);
//     y_pos[i] = r_mm[i] * cosf(theta);
//   }
// }

// void calibrateFsrBaseline() {
//   delay(3000);

//   for (int i = 0; i < NUM_SENSORS; i++) {
//     fsrBaseline[i] = 0.0f;
//   }

//   for (int n = 0; n < FSR_BASELINE_SAMPLES; n++) {
//     for (int i = 0; i < NUM_SENSORS; i++) {
//       fsrBaseline[i] += analogRead(fsrPins[i]);
//     }
//     delay(10);
//   }

//   for (int i = 0; i < NUM_SENSORS; i++) {
//     fsrBaseline[i] /= FSR_BASELINE_SAMPLES;
//   }
// }

// void readFsrActivities(float s[NUM_SENSORS]) {
//   for (int i = 0; i < NUM_SENSORS; i++) {
//     float raw = analogRead(fsrPins[i]);
//     float act = raw - fsrBaseline[i];

//     if (act < 0.0f) act = 0.0f;
//     if (act < FSR_SENSOR_DEADBAND) act = 0.0f;

//     s[i] = act;
//   }
// }

// void computeRawHandPlacementXY(const float s[NUM_SENSORS], float &hp_x, float &hp_y) {
//   float S = 0.0f;
//   float Tx = 0.0f;
//   float Ty = 0.0f;

//   for (int i = 0; i < NUM_SENSORS; i++) {
//     S += s[i];
//     Tx += s[i] * x_pos[i];
//     Ty += s[i] * y_pos[i];
//   }

//   if (S <= 1e-6f) {
//     hp_x = 0.0f;
//     hp_y = 0.0f;
//     return;
//   }

//   hp_x = Tx / S;
//   hp_y = Ty / S;
// }

// void printCsvHeader() {
//   Serial.println("time_ms,depth_mm,hp_x,hp_y");
// }

// void setup() {
//   Serial.begin(115200);
//   while (!Serial) {}

//   setupGeometry();

//   if (!tof_init()) {
//     while (1) {}
//   }

//   if (!tof_tareBaseline(2000)) {
//     while (1) {}
//   }

//   calibrateFsrBaseline();

//   printCsvHeader();
//   lastLogMs = millis();
// }

// void loop() {
//   ToFReading tf = tof_update();

//   unsigned long now = millis();
//   if (now - lastLogMs < LOG_INTERVAL_MS) {
//     return;
//   }
//   lastLogMs += LOG_INTERVAL_MS;

//   float s[NUM_SENSORS];
//   readFsrActivities(s);

//   float hp_x = 0.0f;
//   float hp_y = 0.0f;
//   computeRawHandPlacementXY(s, hp_x, hp_y);

//   Serial.print(now);
//   Serial.print(",");
//   Serial.print(tf.depth_mm);
//   Serial.print(",");
//   Serial.print(hp_x, 2);
//   Serial.print(",");
//   Serial.println(hp_y, 2);
// }




//////////////////////////////////////////////////// raw hp depth testing code, not used in final product /////////////////////////////////////////////

// #include <Arduino.h>
// #include <math.h>

// #include "tof.h"

// // ---------------- FSR setup ----------------
// static const int NUM_SENSORS = 8;

// static const int fsrPins[NUM_SENSORS] = {
//   A8, A9, A10, A11, A12, A13, A14, A15
// };

// static float fsrBaseline[NUM_SENSORS];
// static const int FSR_BASELINE_SAMPLES = 150;
// static const float FSR_SENSOR_DEADBAND = 5.0f;

// // ---------------- Hand placement geometry ----------------
// static const float r_mm[NUM_SENSORS] = {
//   30.0f, 30.96f, 31.24f, 29.85f, 30.0f, 30.25f, 30.28f, 30.93f
// };

// static const float theta_deg[NUM_SENSORS] = {
//   0.0f, 45.0f, 89.0f, 136.0f, 180.0f, 225.0f, 269.0f, 307.0f
// };

// static float x_pos[NUM_SENSORS];
// static float y_pos[NUM_SENSORS];

// // ---------------- Logging ----------------
// static const unsigned long LOG_INTERVAL_MS = 33; // ~30 Hz
// static unsigned long lastLogMs = 0;

// // ---------------- Helpers ----------------
// static float degToRad(float deg) {
//   return deg * (PI / 180.0f);
// }

// void setupGeometry() {
//   for (int i = 0; i < NUM_SENSORS; i++) {
//     float theta = degToRad(theta_deg[i]);
//     x_pos[i] = r_mm[i] * sinf(theta);
//     y_pos[i] = r_mm[i] * cosf(theta);
//   }
// }

// void calibrateFsrBaseline() {
//   delay(3000);

//   for (int i = 0; i < NUM_SENSORS; i++) {
//     fsrBaseline[i] = 0.0f;
//   }

//   for (int n = 0; n < FSR_BASELINE_SAMPLES; n++) {
//     for (int i = 0; i < NUM_SENSORS; i++) {
//       fsrBaseline[i] += analogRead(fsrPins[i]);
//     }
//     delay(10);
//   }

//   for (int i = 0; i < NUM_SENSORS; i++) {
//     fsrBaseline[i] /= FSR_BASELINE_SAMPLES;
//   }
// }

// void readFsrActivities(float s[NUM_SENSORS]) {
//   for (int i = 0; i < NUM_SENSORS; i++) {
//     float raw = analogRead(fsrPins[i]);
//     float act = raw - fsrBaseline[i];

//     if (act < 0.0f) act = 0.0f;
//     if (act < FSR_SENSOR_DEADBAND) act = 0.0f;

//     s[i] = act;
//   }
// }

// void computeRawHandPlacementXY(const float s[NUM_SENSORS], float &hp_x, float &hp_y) {
//   float S = 0.0f;
//   float Tx = 0.0f;
//   float Ty = 0.0f;

//   for (int i = 0; i < NUM_SENSORS; i++) {
//     S += s[i];
//     Tx += s[i] * x_pos[i];
//     Ty += s[i] * y_pos[i];
//   }

//   if (S <= 1e-6f) {
//     hp_x = 0.0f;
//     hp_y = 0.0f;
//     return;
//   }

//   hp_x = Tx / S;
//   hp_y = Ty / S;
// }

// void printCsvHeader() {
//   Serial.println("time_ms,depth_mm,hp_x,hp_y");
// }

// void setup() {
//   Serial.begin(115200);
//   while (!Serial) {}

//   setupGeometry();

//   if (!tof_init()) {
//     while (1) {}
//   }

//   if (!tof_tareBaseline(2000)) {
//     while (1) {}
//   }

//   calibrateFsrBaseline();

//   printCsvHeader();
//   lastLogMs = millis();
// }

// void loop() {
//   ToFReading tf = tof_update();

//   unsigned long now = millis();
//   if (now - lastLogMs < LOG_INTERVAL_MS) {
//     return;
//   }
//   lastLogMs += LOG_INTERVAL_MS;

//   float s[NUM_SENSORS];
//   readFsrActivities(s);

//   float hp_x = 0.0f;
//   float hp_y = 0.0f;
//   computeRawHandPlacementXY(s, hp_x, hp_y);

//   Serial.print(now);
//   Serial.print(",");
//   Serial.print(tf.depth_mm);
//   Serial.print(",");
//   Serial.print(hp_x, 2);
//   Serial.print(",");
//   Serial.println(hp_y, 2);
// }

/////////////////////////////////////////////////////////////// FSR hand placement testing code, not used in final product /////////////////////////////////////////////

// // #include <Arduino.h>
// // #include <math.h>

// // #include "tof.h"

// // // ---------------- FSR setup ----------------
// // static const int NUM_SENSORS = 8;

// // static const int fsrPins[NUM_SENSORS] = {
// //   A8, A9, A10, A11, A12, A13, A14, A15
// // };

// // static float fsrBaseline[NUM_SENSORS];
// // static const int FSR_BASELINE_SAMPLES = 150;
// // static const float FSR_SENSOR_DEADBAND = 5.0f;

// // // ---------------- Logging ----------------
// // static const unsigned long LOG_INTERVAL_MS = 33; // ~30 Hz
// // static unsigned long lastLogMs = 0;

// // // ---------------- Helpers ----------------
// // void calibrateFsrBaseline() {
// //   delay(3000);

// //   for (int i = 0; i < NUM_SENSORS; i++) {
// //     fsrBaseline[i] = 0.0f;
// //   }

// //   for (int n = 0; n < FSR_BASELINE_SAMPLES; n++) {
// //     for (int i = 0; i < NUM_SENSORS; i++) {
// //       fsrBaseline[i] += analogRead(fsrPins[i]);
// //     }
// //     delay(10);
// //   }

// //   for (int i = 0; i < NUM_SENSORS; i++) {
// //     fsrBaseline[i] /= FSR_BASELINE_SAMPLES;
// //   }
// // }

// // void readFsrActivities(float s[NUM_SENSORS]) {
// //   for (int i = 0; i < NUM_SENSORS; i++) {
// //     float raw = analogRead(fsrPins[i]);
// //     float act = raw - fsrBaseline[i];

// //     if (act < 0.0f) act = 0.0f;
// //     if (act < FSR_SENSOR_DEADBAND) act = 0.0f;

// //     s[i] = act;
// //   }
// // }

// // void printCsvHeader() {
// //   Serial.println("time_ms,depth_mm,s0,s1,s2,s3,s4,s5,s6,s7");
// // }

// // void setup() {
// //   Serial.begin(115200);
// //   while (!Serial) {}

// //   if (!tof_init()) {
// //     while (1) {}
// //   }

// //   if (!tof_tareBaseline(2000)) {
// //     while (1) {}
// //   }

// //   calibrateFsrBaseline();

// //   printCsvHeader();
// //   lastLogMs = millis();
// // }

// // void loop() {
// //   ToFReading tf = tof_update();

// //   unsigned long now = millis();
// //   if (now - lastLogMs < LOG_INTERVAL_MS) {
// //     return;
// //   }
// //   lastLogMs += LOG_INTERVAL_MS;

// //   float s[NUM_SENSORS];
// //   readFsrActivities(s);

// //   Serial.print(now);
// //   Serial.print(",");
// //   Serial.print(tf.depth_mm);

// //   for (int i = 0; i < NUM_SENSORS; i++) {
// //     Serial.print(",");
// //     Serial.print(s[i], 1);
// //   }

// //   Serial.println();
// // }

////////////////////////////////////////////////////////////////////////// motor testing code, not used in final product /////////////////////////////////////////////

// #include <Arduino.h>

// // Pins
// const int PIN_PWM  = 3;
// const int PIN_DIR  = 12;
// const int PIN_BRK  = 9;

// // States
// int motorState = 0; // 0 = OFF, 1 = ON

// void setup() {
//   Serial.begin(115200);

//   pinMode(PIN_PWM, OUTPUT);
//   pinMode(PIN_DIR, OUTPUT);
//   pinMode(PIN_BRK, OUTPUT);

//   digitalWrite(PIN_DIR, HIGH);
//   digitalWrite(PIN_BRK, LOW);

//   analogWrite(PIN_PWM, 0);

//   Serial.println("Send 0 (OFF) or 1 (ON)");
// }

// void loop() {
//   if (Serial.available()) {
//     char ch = Serial.read();
//     if (ch == '0') {
//       motorState = 0;
//       Serial.println("OFF");
//     }
//     if (ch == '1') {
//       motorState = 1;
//       Serial.println("ON");
//     }
//   }

//   if (motorState == 0) {
//     analogWrite(PIN_PWM, 0);
//   } else {
//     analogWrite(PIN_PWM, 200);  // full flow
//   }
// }