#include "dbp.h"
#include <math.h>

// Base DBP from depth-only cubic
float dbp_from_depth(float relative_depth_mm) {
  float d2 = powf(relative_depth_mm, 2);
  float d3 = powf(relative_depth_mm, 3);
  return -0.000451f * d3 + 0.030743f * d2 + 0.246959f * relative_depth_mm + 5.0f;
}

// Rate scaling
float rate_scale(float rate_cpm) {
  float scale = 1.0f;

  if (rate_cpm < 60.0f) {
    scale = 1.0f / (1.0f + expf(-0.25f * (rate_cpm - 40.0f)));
  } else if (rate_cpm > 150.0f) {
    scale = -0.0025f * rate_cpm + 1.375f;
  }

  if (scale < 0.0f) scale = 0.0f;
  if (scale > 1.0f) scale = 1.0f;

  return scale;
}

// Lean penalty as multiplicative scale
float lean_scale(float lean_mm) {
  if (lean_mm <= 0.0f) {
    return 1.0f;
  }

  float penalty = 0.0501f * powf(logf(lean_mm + 1.0f), 1.62f);
  float scale = 1.0f - penalty;

  if (scale < 0.0f) scale = 0.0f;
  if (scale > 1.0f) scale = 1.0f;

  return scale;
}

// Main function: plug CPR metrics in, get DBP (mmHg) out.
float compute_dbp_mmHg(const CprMetrics &m) {
  float dbp = dbp_from_depth(m.relative_depth_mm);

  dbp = dbp * rate_scale(m.rate_cpm)
            * lean_scale(m.lean_mm)
            * m.pause_scale;

  return dbp;
}