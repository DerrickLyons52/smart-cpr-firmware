#pragma once
#include <Arduino.h>

struct LoadCellReading {
  bool ok;
  long raw;        // raw HX711 counts (filtered)
};

bool loadcell_init(uint8_t dout_pin, uint8_t sck_pin);
LoadCellReading loadcell_read();