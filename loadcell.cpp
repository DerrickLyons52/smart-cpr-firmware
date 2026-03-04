#include "loadcell.h"
#include <HX711.h>
#include <limits.h>

static HX711 scale;
static bool inited = false;

static long readMed3() {
  long a = scale.read(), b = scale.read(), c = scale.read();
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

// simple robust filter: average of a few median-of-3 blocks
static long readFiltered(int blocks = 3) {
  long long sum = 0;
  for (int i = 0; i < blocks; i++) {
    if (!scale.wait_ready_timeout(1000)) return LONG_MIN;
    sum += readMed3();
  }
  return (long)(sum / blocks);
}

bool loadcell_init(uint8_t dout_pin, uint8_t sck_pin) {
  scale.begin(dout_pin, sck_pin);
  inited = scale.wait_ready_timeout(3000);
  return inited;
}

LoadCellReading loadcell_read() {
  LoadCellReading out{};
  if (!inited) { out.ok = false; out.raw = 0; return out; }

  long v = readFiltered(3);
  if (v == LONG_MIN) { out.ok = false; out.raw = 0; return out; }

  out.ok = true;
  out.raw = v;
  return out;
}