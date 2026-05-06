#pragma once
#include <Arduino.h>
#include "tof.h"

enum class CprActivityState {
  ACTIVE,
  PAUSED
};

enum class DepthState {
  SHALLOW,
  GOOD,
  DEEP
};

enum class RateState {
  SLOW,
  GOOD,
  FAST
};

struct CompressionMetrics {
  bool compression_started = false;   // true only on the update where a new compression starts
  bool compression_completed = false; // true only when a compression cycle finishes

  CprActivityState activity_state = CprActivityState::PAUSED;
  DepthState depth_state = DepthState::SHALLOW;
  RateState rate_state = RateState::SLOW;

  float filtered_depth_mm = 0.0f;

  float start_depth_mm = 0.0f;     // lean for current/last compression
  float peak_depth_mm = 0.0f;      // max absolute depth
  float relative_depth_mm = 0.0f;  // peak - start
  float lean_mm = 0.0f;

  float rate_cpm = 0.0f;

  unsigned long last_compression_time_ms = 0;
};

bool compressionAnalysisInit();
CompressionMetrics compressionAnalysisUpdate(const ToFReading& tf);