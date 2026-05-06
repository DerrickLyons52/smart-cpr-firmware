// #include "tof_VL53L1X.h"
// #include <Wire.h>
// #include <VL53L1X.h>

// static VL53L1X sensor;
// static bool inited = false;

// bool tof_init() {
//   Wire.begin();
//   Wire.setClock(400000);

//   sensor.setTimeout(500);

//   if (!sensor.init()) {
//     inited = false;
//     return false;
//   }

//   sensor.setDistanceMode(VL53L1X::Short);
//   sensor.setMeasurementTimingBudget(25000);
//   sensor.startContinuous(50);

//   inited = true;
//   return true;
// }

// ToFReading tof_read() {
//   ToFReading out{};

//   if (!inited) {
//     out.ok = false;
//     out.distance_mm = 0;
//     return out;
//   }

//   uint16_t d = sensor.read();

//   if (sensor.timeoutOccurred()) {
//     out.ok = false;
//     out.distance_mm = 0;
//     return out;
//   }

//   out.ok = true;
//   out.distance_mm = d;
//   return out;
// }