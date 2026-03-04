#include "hand_placement.h"
#include <math.h>

static inline float clamp0(float v) { return (v < 0.0f) ? 0.0f : v; }

// Helper: classify direction using a single threshold.
// Prompts are opposite direction: if pressure biased right (x>0), move left.
static HandPrompt classifyPrompt(float x, float y, float thr) {
  const bool x_hi = (x >  thr);
  const bool x_lo = (x < -thr);
  const bool y_hi = (y >  thr);
  const bool y_lo = (y < -thr);

  if (!x_hi && !x_lo && !y_hi && !y_lo) {
    return HandPrompt::OK;
  } else if (y_lo && x_lo) {
    return HandPrompt::MOVE_UP_RIGHT;
  } else if (y_lo && x_hi) {
    return HandPrompt::MOVE_UP_LEFT;
  } else if (y_hi && x_lo) {
    return HandPrompt::MOVE_DOWN_RIGHT;
  } else if (y_hi && x_hi) {
    return HandPrompt::MOVE_DOWN_LEFT;
  } else if (x_hi) {
    return HandPrompt::MOVE_LEFT;
  } else if (x_lo) {
    return HandPrompt::MOVE_RIGHT;
  } else if (y_hi) {
    return HandPrompt::MOVE_DOWN;
  } else { // y_lo
    return HandPrompt::MOVE_UP;
  }
}

HandPlacementResult computeHandPlacement(
  const float s_in[8],
  float r,
  float tau,
  float S_min
) {
  // Map s_in[0]..s_in[7] to s1..s8
  const float s1 = clamp0(s_in[0]);
  const float s2 = clamp0(s_in[1]);
  const float s3 = clamp0(s_in[2]);
  const float s4 = clamp0(s_in[3]);
  const float s5 = clamp0(s_in[4]);
  const float s6 = clamp0(s_in[5]);
  const float s7 = clamp0(s_in[6]);
  const float s8 = clamp0(s_in[7]);

  HandPlacementResult out{};
  out.S  = s1+s2+s3+s4+s5+s6+s7+s8;

  // Raw directional imbalance
  out.Tx = r * ((s2+s3+s4) - (s6+s7+s8));
  out.Ty = r * ((s1+s2+s8) - (s4+s5+s6));

  if (out.S < S_min) {
    out.x = 0.0f;
    out.y = 0.0f;
    out.prompt = HandPrompt::NO_HANDS;
    return out;
  }

  out.x = out.Tx / out.S;
  out.y = out.Ty / out.S;

  // NOTE: this preserves your original thresholding behavior.
  const float thr = r * tau;

  out.prompt = classifyPrompt(out.x, out.y, thr);
  return out;
}

HandPlacementResult computeHandPlacementHysteresis(
  const float s_in[8],
  float r,
  float tau_enter,
  float tau_exit,
  float S_min,
  HandPlacementState* state
) {
  // Safety: if no state provided, fall back to non-hysteresis
  if (!state) {
    return computeHandPlacement(s_in, r, tau_enter, S_min);
  }

  // Map s_in[0]..s_in[7] to s1..s8
  const float s1 = clamp0(s_in[0]);
  const float s2 = clamp0(s_in[1]);
  const float s3 = clamp0(s_in[2]);
  const float s4 = clamp0(s_in[3]);
  const float s5 = clamp0(s_in[4]);
  const float s6 = clamp0(s_in[5]);
  const float s7 = clamp0(s_in[6]);
  const float s8 = clamp0(s_in[7]);

  HandPlacementResult out{};
  out.S  = s1+s2+s3+s4+s5+s6+s7+s8;

  // Raw directional imbalance
  out.Tx = r * ((s2+s3+s4) - (s6+s7+s8));
  out.Ty = r * ((s1+s2+s8) - (s4+s5+s6));

  if (out.S < S_min) {
    out.x = 0.0f;
    out.y = 0.0f;
    out.prompt = HandPrompt::NO_HANDS;

    // Reset hysteresis state when hands are gone
    state->correcting = false;
    state->last_prompt = HandPrompt::NO_HANDS;
    return out;
  }

  out.x = out.Tx / out.S;
  out.y = out.Ty / out.S;

  // Thresholds (enter > exit)
  // NOTE: preserves your existing "thr = r * tau" convention.
  const float thr_enter = r * tau_enter;
  const float thr_exit  = r * tau_exit;

  const float ax = fabsf(out.x);
  const float ay = fabsf(out.y);

  const bool outside_enter = (ax > thr_enter) || (ay > thr_enter);
  const bool inside_exit   = (ax < thr_exit)  && (ay < thr_exit);

  if (!state->correcting) {
    // We are currently OK; only enter correction mode if outside ENTER threshold
    if (outside_enter) {
      state->correcting = true;
      state->last_prompt = classifyPrompt(out.x, out.y, thr_enter);
    } else {
      state->last_prompt = HandPrompt::OK;
    }
  } else {
    // We are currently correcting; only return to OK if inside EXIT threshold
    if (inside_exit) {
      state->correcting = false;
      state->last_prompt = HandPrompt::OK;
    } else if (outside_enter) {
      // Clearly off-center again -> allow direction update
      state->last_prompt = classifyPrompt(out.x, out.y, thr_enter);
    }
    // else: in the hysteresis band -> keep last_prompt latched (prevents flicker)
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
    case HandPrompt::NO_HANDS: return "No hands / insufficient force";
    default: return "Unknown";
  }
}