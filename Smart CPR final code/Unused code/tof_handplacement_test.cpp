// #include <Arduino.h>
// #include "tof.h"
// #include "hand_placement.h"

// // ---- Hand placement test input pins (same 4-pot prototype) ----
// static const int PIN_45  = A0; // s2
// static const int PIN_135 = A1; // s4
// static const int PIN_225 = A2; // s6
// static const int PIN_315 = A3; // s8

// // ---- Hand placement parameters ----
// static float r = 1.0f;
// static float tau_enter = 0.15f;
// static float tau_exit  = 0.10f;
// static float S_min     = 20.0f;

// static HandPlacementState hp_state;

// void setup() {
//   Serial.begin(115200);
//   while (!Serial) {}

//   Serial.println("=== ToF + Hand Placement Test ===");

//   bool tof_ok = tof_init();
//   Serial.print("ToF init: ");
//   Serial.println(tof_ok ? "OK" : "FAILED");
// }

// void loop() {
//   // ---------------- ToF ----------------
//   ToFReading tf = tof_read();

//   // ---------------- Hand placement ----------------
//   float s[8] = {0};

//   // Only using 4 prototype channels for now:
//   s[1] = analogRead(PIN_45);   // s2
//   s[3] = analogRead(PIN_135);  // s4
//   s[5] = analogRead(PIN_225);  // s6
//   s[7] = analogRead(PIN_315);  // s8

//   HandPlacementResult hp = computeHandPlacementHysteresis(
//     s,
//     r,
//     tau_enter,
//     tau_exit,
//     S_min,
//     &hp_state
//   );

//   // ---------------- Output ----------------
//   Serial.print("ToF(mm)=");
//   if (tf.ok) Serial.print(tf.distance_mm);
//   else       Serial.print("ERR");

//   Serial.print(" | S=");
//   Serial.print(hp.S, 1);

//   Serial.print(" x=");
//   Serial.print(hp.x, 3);

//   Serial.print(" y=");
//   Serial.print(hp.y, 3);

//   Serial.print(" | Prompt=");
//   Serial.println(promptToString(hp.prompt));

//   delay(200);
// }