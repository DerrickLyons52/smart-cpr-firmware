#include "compression_analysis.h"

// ---------------- Tuning ----------------
static const float FILTER_ALPHA = 0.25f;

static const float START_RISE_MM = 7.0f;   // start when filtered depth rises from recoil min
static const float PEAK_DROP_MM  = 5.0f;   // complete when filtered depth drops from filtered peak

static const float MAX_DEPTH_MM = 63.5f;   // 2.5 inches mechanical limit

// ---------------- Internal state ----------------
static bool initialized = false;

static CompressionMotionState motionState = CompressionMotionState::IDLE;

static float filteredDepth = 0.0f;
static float startDepth = 0.0f;

static float peakDepth = 0.0f;          // raw peak, capped at 63.5 mm
static float filteredPeakDepth = 0.0f;  // filtered peak, used only for state detection

static float recoilMinDepth = 0.0f;

static unsigned long lastCompressionTimeMs = 0;

static CompressionMetrics lastMetrics;

static float clampDepth(float depth) {
  if (depth < 0.0f) return 0.0f;
  if (depth > MAX_DEPTH_MM) return MAX_DEPTH_MM;
  return depth;
}

void compressionAnalysisInit() {
  initialized = true;

  motionState = CompressionMotionState::IDLE;

  filteredDepth = 0.0f;
  startDepth = 0.0f;
  peakDepth = 0.0f;
  filteredPeakDepth = 0.0f;
  recoilMinDepth = 0.0f;

  lastCompressionTimeMs = 0;

  lastMetrics = CompressionMetrics{};
  lastMetrics.motion_state = motionState;
}

static void startCompression(CompressionMetrics& out) {
  motionState = CompressionMotionState::COMPRESSING;

  startDepth = recoilMinDepth;

  peakDepth = out.raw_depth_mm;
  filteredPeakDepth = filteredDepth;

  out.compression_started = true;
  out.start_depth_mm = startDepth;
  out.lean_mm = startDepth;
}

CompressionMetrics compressionAnalysisUpdate(const ToFReading& tf) {
  if (!initialized) {
    compressionAnalysisInit();
  }

  CompressionMetrics out = lastMetrics;

  out.compression_started = false;
  out.compression_completed = false;

  if (!tf.new_data || !tf.ok) {
    out.motion_state = motionState;
    lastMetrics = out;
    return out;
  }

  float rawDepth = clampDepth(static_cast<float>(tf.depth_mm));

  filteredDepth = FILTER_ALPHA * rawDepth + (1.0f - FILTER_ALPHA) * filteredDepth;
  filteredDepth = clampDepth(filteredDepth);

  out.raw_depth_mm = rawDepth;
  out.filtered_depth_mm = filteredDepth;

  switch (motionState) {
    case CompressionMotionState::IDLE:
      if (filteredDepth < recoilMinDepth || recoilMinDepth == 0.0f) {
        recoilMinDepth = filteredDepth;
      }

      if ((filteredDepth - recoilMinDepth) >= START_RISE_MM) {
        startCompression(out);
      }
      break;

    case CompressionMotionState::COMPRESSING:
      // Raw peak for actual reported compression depth
      if (rawDepth > peakDepth) {
        peakDepth = rawDepth;
      }

      // Filtered peak only for stable release detection
      if (filteredDepth > filteredPeakDepth) {
        filteredPeakDepth = filteredDepth;
      }

      if ((filteredPeakDepth - filteredDepth) >= PEAK_DROP_MM) {
        motionState = CompressionMotionState::RELEASING;

        unsigned long now = millis();

        if (lastCompressionTimeMs != 0) {
          unsigned long dt = now - lastCompressionTimeMs;
          if (dt > 0) {
            out.rate_cpm = 60000.0f / static_cast<float>(dt);
          }
        }

        lastCompressionTimeMs = now;

        out.compression_completed = true;
        out.last_compression_time_ms = lastCompressionTimeMs;

        out.start_depth_mm = startDepth;
        out.peak_depth_mm = peakDepth;
        out.lean_mm = startDepth;

        recoilMinDepth = filteredDepth;
      }
      break;

    case CompressionMotionState::RELEASING:
      if (filteredDepth < recoilMinDepth) {
        recoilMinDepth = filteredDepth;
      }

      if ((filteredDepth - recoilMinDepth) >= START_RISE_MM) {
        startCompression(out);
      }
      break;
  }

  out.motion_state = motionState;
  lastMetrics = out;
  return out;
}