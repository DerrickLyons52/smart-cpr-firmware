#pragma once
#include <Arduino.h>
#include "tof.h"

enum class CompressionMotionState {
  IDLE,
  COMPRESSING,
  RELEASING
};

struct CompressionMetrics {
  // Current depth values
  float raw_depth_mm = 0.0f;
  float filtered_depth_mm = 0.0f;

  // One-loop event flags
  bool compression_started = false;
  bool compression_completed = false;

  // Compression measurements
  float start_depth_mm = 0.0f;  // depth at beginning of compression
  float peak_depth_mm = 0.0f;   // deepest point reached
  float lean_mm = 0.0f;         // same idea as start_depth_mm

  // Timing
  float rate_cpm = 0.0f;
  unsigned long last_compression_time_ms = 0;

  // Debug/state
  CompressionMotionState motion_state = CompressionMotionState::IDLE;
};

void compressionAnalysisInit();

CompressionMetrics compressionAnalysisUpdate(const ToFReading& tf);