#pragma once
#include <Arduino.h>

struct ToFReading {
  bool ok = false;                    // reading is usable
  bool new_data = false;              // true if a fresh measurement was read
  uint16_t raw_distance_mm = 0;       // direct sensor reading
  uint16_t corrected_distance_mm = 0; // offset-corrected reading
  uint16_t baseline_distance_mm = 0;  // tared baseline distance
  int depth_mm = 0;                   // compression depth relative to baseline
  uint8_t range_status = 255;         // sensor status code
  uint16_t signal_per_spad_kcps = 0;  // signal strength
};

// Initialize the VL53L4CD and start ranging
bool tof_init();

// Tare the baseline chest distance while chest is still
bool tof_tareBaseline(unsigned long tare_time_ms = 2000);

// Read the latest measurement and convert it to depth
ToFReading tof_update();