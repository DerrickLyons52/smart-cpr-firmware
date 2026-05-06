#include "hand_placement.h"
#include <math.h>

static const int NUM_SENSORS = 8;

// Measured geometry from hardware test
static const float r_mm[NUM_SENSORS] = {
  30.0f, 30.96f, 31.24f, 29.85f, 30.0f, 30.25f, 30.28f, 30.93f
};

static const float theta_deg[NUM_SENSORS] = {
  0.0f, 45.0f, 89.0f, 136.0f, 180.0f, 225.0f, 269.0f, 307.0f
};

static float x_pos[NUM_SENSORS];
static float y_pos[NUM_SENSORS];
static bool geometry_initialized = false;

static inline float clamp0(float v) {
  return (v < 0.0f) ? 0.0f : v;
}

static inline float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static float degToRad(float deg) {
  return deg * (PI / 180.0f);
}

void handPlacementInitGeometry() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    float theta = degToRad(theta_deg[i]);

    // 0° at top, clockwise positive
    x_pos[i] = r_mm[i] * sinf(theta);
    y_pos[i] = r_mm[i] * cosf(theta);
  }

  geometry_initialized = true;
}

// Prompt is opposite the pressure bias:
// if pressure is biased right (x > 0), user should move left.
static HandPrompt classifyPrompt(float x, float y, float thr_mm) {
  const bool x_hi = (x >  thr_mm);
  const bool x_lo = (x < -thr_mm);
  const bool y_hi = (y >  thr_mm);
  const bool y_lo = (y < -thr_mm);

  if (!x_hi && !x_lo && !y_hi && !y_lo) return HandPrompt::OK;

  if (y_lo && x_lo) return HandPrompt::MOVE_UP_RIGHT;
  if (y_lo && x_hi) return HandPrompt::MOVE_UP_LEFT;
  if (y_hi && x_lo) return HandPrompt::MOVE_DOWN_RIGHT;
  if (y_hi && x_hi) return HandPrompt::MOVE_DOWN_LEFT;

  if (x_hi) return HandPrompt::MOVE_LEFT;
  if (x_lo) return HandPrompt::MOVE_RIGHT;
  if (y_hi) return HandPrompt::MOVE_DOWN;
  return HandPrompt::MOVE_UP;
}

// Geometric/system-design model, not a paper-derived physiology equation.
static void computePlacementPenalty(
  float offset_mm,
  float ok_radius_mm,
  float mild_radius_mm,
  float moderate_radius_mm,
  float* severity,
  float* effectiveness
) {
  if (!severity || !effectiveness) return;

  if (offset_mm <= ok_radius_mm) {
    *severity = 0.0f;
    *effectiveness = 1.0f;
  } else if (offset_mm <= mild_radius_mm) {
    float t = (offset_mm - ok_radius_mm) / (mild_radius_mm - ok_radius_mm);
    t = clamp01(t);

    *severity = 0.10f + 0.20f * t;       // 0.10 -> 0.30
    *effectiveness = 0.95f - 0.10f * t;  // 0.95 -> 0.85
  } else if (offset_mm <= moderate_radius_mm) {
    float t = (offset_mm - mild_radius_mm) / (moderate_radius_mm - mild_radius_mm);
    t = clamp01(t);

    *severity = 0.30f + 0.40f * t;       // 0.30 -> 0.70
    *effectiveness = 0.85f - 0.35f * t;  // 0.85 -> 0.50
  } else {
    *severity = 1.0f;
    *effectiveness = 0.0f;
  }
}

HandPlacementResult computeHandPlacement(
  const float s_in[8],
  float sensor_deadband,
  float alpha,
  float tau_enter_mm,
  float tau_exit_mm,
  float ok_radius_mm,
  float mild_radius_mm,
  float moderate_radius_mm,
  HandPlacementState* state
) {
  static HandPlacementState fallback_state;
  if (!state) {
    state = &fallback_state;
  }

  if (!geometry_initialized) {
    handPlacementInitGeometry();
  }

  HandPlacementResult out{};
  float act[NUM_SENSORS];

  out.S = 0.0f;
  out.Tx = 0.0f;
  out.Ty = 0.0f;

  // Read processed sensor activities and compute weighted position
  for (int i = 0; i < NUM_SENSORS; i++) {
    act[i] = clamp0(s_in[i]);

    if (act[i] < sensor_deadband) {
      act[i] = 0.0f;
    }

    out.S += act[i];
    out.Tx += act[i] * x_pos[i];
    out.Ty += act[i] * y_pos[i];
  }

  // Avoid divide-by-zero, but do not create a no-hands state here.
  if (out.S <= 1e-6f) {
    out.x = 0.0f;
    out.y = 0.0f;
    out.xf = 0.0f;
    out.yf = 0.0f;
    out.offset_mm = 0.0f;
    out.severity = 0.0f;
    out.effectiveness = 1.0f;
    out.prompt = HandPrompt::OK;

    state->filter_initialized = false;
    state->correcting = false;
    state->xf = 0.0f;
    state->yf = 0.0f;
    state->last_prompt = HandPrompt::OK;
    return out;
  }

  out.x = out.Tx / out.S;
  out.y = out.Ty / out.S;

  // Filter centroid
  if (!state->filter_initialized) {
    state->xf = out.x;
    state->yf = out.y;
    state->filter_initialized = true;
  } else {
    state->xf = alpha * out.x + (1.0f - alpha) * state->xf;
    state->yf = alpha * out.y + (1.0f - alpha) * state->yf;
  }

  out.xf = state->xf;
  out.yf = state->yf;
  out.offset_mm = sqrtf(out.xf * out.xf + out.yf * out.yf);

  computePlacementPenalty(
    out.offset_mm,
    ok_radius_mm,
    mild_radius_mm,
    moderate_radius_mm,
    &out.severity,
    &out.effectiveness
  );

  const float ax = fabsf(out.xf);
  const float ay = fabsf(out.yf);

  const bool outside_enter = (ax > tau_enter_mm) || (ay > tau_enter_mm);
  const bool inside_exit   = (ax < tau_exit_mm)  && (ay < tau_exit_mm);

  if (!state->correcting) {
    if (outside_enter) {
      state->correcting = true;
      state->last_prompt = classifyPrompt(out.xf, out.yf, tau_enter_mm);
    } else {
      state->last_prompt = HandPrompt::OK;
    }
  } else {
    if (inside_exit) {
      state->correcting = false;
      state->last_prompt = HandPrompt::OK;
    } else if (outside_enter) {
      state->last_prompt = classifyPrompt(out.xf, out.yf, tau_enter_mm);
    }
    // else: remain latched in hysteresis band
  }

  out.prompt = state->last_prompt;
  return out;
}

const char* promptToString(HandPrompt p) {
  switch (p) {
    case HandPrompt::OK: return "OK";
    case HandPrompt::MOVE_LEFT: return "Move LEFT";
    case HandPrompt::MOVE_RIGHT: return "Move RIGHT";
    case HandPrompt::MOVE_UP: return "Move UP";
    case HandPrompt::MOVE_DOWN: return "Move DOWN";
    case HandPrompt::MOVE_UP_LEFT: return "Move UP-LEFT";
    case HandPrompt::MOVE_UP_RIGHT: return "Move UP-RIGHT";
    case HandPrompt::MOVE_DOWN_LEFT: return "Move DOWN-LEFT";
    case HandPrompt::MOVE_DOWN_RIGHT: return "Move DOWN-RIGHT";
    default: return "Unknown";
  }
}