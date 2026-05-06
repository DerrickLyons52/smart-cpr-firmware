#include "compression_analysis_old.h"
#include <math.h>

namespace {

// ---------- Tunable parameters ----------
static const float ALPHA = 0.25f;                  // depth low-pass filter
static const float MOTION_START_THRESH_MM = 3.0f; // depth increase to declare compression motion
static const float PEAK_DROP_THRESH_MM = 2.0f;    // drop from peak to confirm release
static const float RECOIL_NEAR_TOP_MM = 5.0f;     // near-top region
static const unsigned long PAUSE_TIMEOUT_MS = 3000;

// Depth coaching thresholds (for relative depth)
static const float DEPTH_SHALLOW_MM = 40.0f;
static const float DEPTH_DEEP_MM = 60.0f;

// Rate coaching thresholds
static const float RATE_SLOW_CPM = 100.0f;
static const float RATE_FAST_CPM = 120.0f;

// ---------- Internal state ----------
enum class MotionState {
  IDLE_TOP,
  COMPRESSING,
  RELEASING
};

static bool initialized = false;
static MotionState motion_state = MotionState::IDLE_TOP;

static float filt_depth_mm = 0.0f;
static float prev_filt_depth_mm = 0.0f;

static float current_start_depth_mm = 0.0f;
static float current_peak_depth_mm = 0.0f;

static unsigned long last_update_ms = 0;
static unsigned long last_peak_time_ms = 0;

static CompressionMetrics last_metrics;

} // namespace

bool compressionAnalysisInit() {
  initialized = true;
  motion_state = MotionState::IDLE_TOP;

  filt_depth_mm = 0.0f;
  prev_filt_depth_mm = 0.0f;

  current_start_depth_mm = 0.0f;
  current_peak_depth_mm = 0.0f;

  last_update_ms = millis();
  last_peak_time_ms = 0;

  last_metrics = CompressionMetrics{};
  last_metrics.activity_state = CprActivityState::PAUSED;
  last_metrics.depth_state = DepthState::SHALLOW;
  last_metrics.rate_state = RateState::SLOW;

  return true;
}

CompressionMetrics compressionAnalysisUpdate(const ToFReading& tf) {
  CompressionMetrics out = last_metrics;
  out.compression_started = false;
  out.compression_completed = false;

  const unsigned long now_ms = millis();

  if (!initialized) {
    compressionAnalysisInit();
  }

  // If no new usable sensor data, just maintain pause/activity timing.
  if (!tf.new_data || !tf.ok) {
    if ((now_ms - out.last_compression_time_ms) > PAUSE_TIMEOUT_MS) {
      out.activity_state = CprActivityState::PAUSED;
    }
    last_metrics = out;
    return out;
  }

  // Filter depth
  const float raw_depth = (tf.depth_mm < 0) ? 0.0f : static_cast<float>(tf.depth_mm);

  if (last_update_ms == 0) {
    filt_depth_mm = raw_depth;
    prev_filt_depth_mm = raw_depth;
  } else {
    prev_filt_depth_mm = filt_depth_mm;
    filt_depth_mm = ALPHA * raw_depth + (1.0f - ALPHA) * filt_depth_mm;
  }

  out.filtered_depth_mm = filt_depth_mm;

  const float ddepth = filt_depth_mm - prev_filt_depth_mm;

  switch (motion_state) {
    case MotionState::IDLE_TOP:
      // Detect start of a new compression when depth clearly increases from near top
      if (filt_depth_mm >= MOTION_START_THRESH_MM && ddepth > 0.5f) {
        motion_state = MotionState::COMPRESSING;

        current_start_depth_mm = prev_filt_depth_mm;
        if (current_start_depth_mm < 0.0f) current_start_depth_mm = 0.0f;

        current_peak_depth_mm = filt_depth_mm;

        out.compression_started = true;
        out.start_depth_mm = current_start_depth_mm;
        out.lean_mm = current_start_depth_mm;
      }
      break;

    case MotionState::COMPRESSING:
      // Track peak while depth is increasing
      if (filt_depth_mm > current_peak_depth_mm) {
        current_peak_depth_mm = filt_depth_mm;
      }

      // If depth has fallen enough from peak, we are releasing
      if ((current_peak_depth_mm - filt_depth_mm) >= PEAK_DROP_THRESH_MM) {
        motion_state = MotionState::RELEASING;

        // Peak time marks a completed compression for rate
        if (last_peak_time_ms != 0) {
          const unsigned long dt_ms = now_ms - last_peak_time_ms;
          if (dt_ms > 0) {
            out.rate_cpm = 60000.0f / static_cast<float>(dt_ms);
          }
        }
        last_peak_time_ms = now_ms;
        out.last_compression_time_ms = now_ms;
        out.activity_state = CprActivityState::ACTIVE;

        out.peak_depth_mm = current_peak_depth_mm;
        out.start_depth_mm = current_start_depth_mm;
        out.lean_mm = current_start_depth_mm;
        out.relative_depth_mm = current_peak_depth_mm - current_start_depth_mm;
        if (out.relative_depth_mm < 0.0f) out.relative_depth_mm = 0.0f;

        out.compression_completed = true;
      }
      break;

    case MotionState::RELEASING:
      // Once chest gets near the top again, re-arm for the next compression
      if (filt_depth_mm <= RECOIL_NEAR_TOP_MM) {
        motion_state = MotionState::IDLE_TOP;
      }
      break;
  }

  // Activity / pause state
  if ((now_ms - out.last_compression_time_ms) > PAUSE_TIMEOUT_MS) {
    out.activity_state = CprActivityState::PAUSED;
  } else {
    out.activity_state = CprActivityState::ACTIVE;
  }

  // Depth state based on most recently completed compression's relative depth
  if (out.relative_depth_mm < DEPTH_SHALLOW_MM) {
    out.depth_state = DepthState::SHALLOW;
  } else if (out.relative_depth_mm > DEPTH_DEEP_MM) {
    out.depth_state = DepthState::DEEP;
  } else {
    out.depth_state = DepthState::GOOD;
  }

  // Rate state
  if (out.rate_cpm < RATE_SLOW_CPM) {
    out.rate_state = RateState::SLOW;
  } else if (out.rate_cpm > RATE_FAST_CPM) {
    out.rate_state = RateState::FAST;
  } else {
    out.rate_state = RateState::GOOD;
  }

  last_update_ms = now_ms;
  last_metrics = out;
  return out;
}