#include "dbp.h"
#include <math.h>

// Base DBP from depth-only cubic
float dbp_from_depth(float depth_mm) {
  float d2 = powf(depth_mm, 2);
  float d3 = powf(depth_mm, 3);
  return -0.000451f * d3 + 0.030743f * d2 + 0.246959f * depth_mm + 5.0f;
}

// Rate scaling
float rate_scale(float rate_cpm) {
  float scale = 1.0f;
  if (rate_cpm < 60.0f) {
    scale = 1.0f / (1.0f + expf(-0.25f * (rate_cpm - 40.0f)));
  } else if (rate_cpm > 150.0f) {
    scale = -0.0025f * rate_cpm + 1.375f;
  }
  return scale;
}

// Lean “penalty” as a multiplicative scale
float lean_scale(float lean_mm) {
  if (lean_mm <= 0.0f) {
    return 1.0f;  // no lean → no penalty
  }
  float penalty = 0.0501f * powf(logf(lean_mm + 1.0f), 1.62f);
  float scale = 1.0f - penalty;        // turn penalty into a % reduction
  if (scale < 0.0f) scale = 0.0f;      // don’t go negative
  return scale;
}


// Hand placement scaling
float placement_scale(float hand_placement_mm) {
  float scale;
  if (hand_placement_mm <= 1.0f) {
    scale = 1.0f;
  } else {
    scale = powf(hand_placement_mm, -2.0f);
  }
  return scale;
}

// Main function: plug CPR metrics in, get DBP (mmHg) out.
float compute_dbp_mmHg(const CprMetrics &m) {
  // 1) base DBP from depth
  float dbp = dbp_from_depth(m.depth_mm);

  // 2) apply scaling (rate, placement, lean)
  dbp = dbp * rate_scale(m.rate_cpm)
             * placement_scale(m.hand_placement_mm)
             * lean_scale(m.lean_mm);

  // 3) pause penalty: 33% reduction when paused
  if (m.paused) {
    dbp *= 0.67f;   // 33% penalty
  }

  return dbp;       // mmHg
}
