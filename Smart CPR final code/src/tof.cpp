#include "tof.h"
#include <Wire.h>
#include <vl53l4cd_class.h>

#define DEV_I2C Wire

static VL53L4CD sensor(&DEV_I2C, 0);

static const int DISTANCE_OFFSET_MM = 15;
static const uint16_t RANGE_TIMING_MS = 33;

static uint16_t baseline_distance_mm = 0;
static bool baseline_valid = false;

static int applyOffset(int raw_mm) {
  int corrected = raw_mm - DISTANCE_OFFSET_MM;
  if (corrected < 0) corrected = 0;
  return corrected;
}

bool tof_init() {
  DEV_I2C.begin();
  sensor.begin();

  if (sensor.InitSensor() != 0) {
    return false;
  }

  sensor.VL53L4CD_SetRangeTiming(RANGE_TIMING_MS, 0);
  sensor.VL53L4CD_StartRanging();

  baseline_valid = false;
  baseline_distance_mm = 0;

  return true;
}

bool tof_tareBaseline(unsigned long tare_time_ms) {
  const unsigned long start_ms = millis();

  uint32_t sum = 0;
  uint16_t count = 0;

  while ((millis() - start_ms) < tare_time_ms) {
    uint8_t newDataReady = 0;
    uint8_t status = sensor.VL53L4CD_CheckForDataReady(&newDataReady);

    if ((status == 0) && (newDataReady != 0)) {
      VL53L4CD_Result_t results;
      sensor.VL53L4CD_ClearInterrupt();
      sensor.VL53L4CD_GetResult(&results);

      int corrected = applyOffset(results.distance_mm);

      // Optionally reject bad statuses here later
      sum += corrected;
      count++;
    }
  }

  if (count == 0) {
    baseline_valid = false;
    return false;
  }

  baseline_distance_mm = static_cast<uint16_t>(sum / count);
  baseline_valid = true;
  return true;
}

ToFReading tof_update() {
  ToFReading out{};
  out.baseline_distance_mm = baseline_distance_mm;

  uint8_t newDataReady = 0;
  uint8_t status = sensor.VL53L4CD_CheckForDataReady(&newDataReady);

  if ((status != 0) || (newDataReady == 0)) {
    return out;
  }

  VL53L4CD_Result_t results;
  sensor.VL53L4CD_ClearInterrupt();
  sensor.VL53L4CD_GetResult(&results);

  out.new_data = true;
  out.raw_distance_mm = results.distance_mm;
  out.corrected_distance_mm = applyOffset(results.distance_mm);
  out.range_status = results.range_status;
  out.signal_per_spad_kcps = results.signal_per_spad_kcps;

  // For now, treat range_status 0 as valid/good.
  // You can broaden this later if needed after testing.
  out.ok = baseline_valid && (results.range_status == 0);

  if (baseline_valid) {
    int depth = static_cast<int>(baseline_distance_mm) - static_cast<int>(out.corrected_distance_mm);
    if (depth < 0) depth = 0;
    out.depth_mm = depth;
  }

  return out;
}