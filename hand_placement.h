#pragma once
#include <Arduino.h>

enum class HandPrompt {
  OK,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_UP,
  MOVE_DOWN,
  MOVE_UP_LEFT,
  MOVE_UP_RIGHT,
  MOVE_DOWN_LEFT,
  MOVE_DOWN_RIGHT,
  NO_HANDS
};

struct HandPlacementResult {
  float S;     // total force (arb units)
  float Tx;    // x numerator (arb units)
  float Ty;    // y numerator (arb units)
  float x;     // normalized x offset
  float y;     // normalized y offset
  HandPrompt prompt;
};

// Hysteresis state (persist across loop iterations)
struct HandPlacementState {
  bool correcting = false;           // are we currently issuing corrective prompts?
  HandPrompt last_prompt = HandPrompt::OK; // latched prompt to avoid flicker
};

// Your existing (non-hysteresis) implementation
HandPlacementResult computeHandPlacement(
  const float s[8],
  float r,
  float tau,
  float S_min
);

// New: hysteresis version
// tau_enter: threshold to ENTER correction mode (bigger)
// tau_exit : threshold to EXIT correction mode (smaller)
// Recommend: tau_exit < tau_enter
HandPlacementResult computeHandPlacementHysteresis(
  const float s[8],
  float r,
  float tau_enter,
  float tau_exit,
  float S_min,
  HandPlacementState* state
);

const char* promptToString(HandPrompt p);