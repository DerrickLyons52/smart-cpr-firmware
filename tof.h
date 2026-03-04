#pragma once
#include <Arduino.h>

struct ToFReading {
  bool ok;
  uint16_t distance_mm;
};

bool tof_init();         // uses Wire + sensor init
ToFReading tof_read();   // returns latest reading