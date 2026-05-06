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
  MOVE_DOWN_RIGHT
};

struct HandPlacementResult {
  float S;              // total activation (arb units)
  float Tx;             // x numerator
  float Ty;             // y numerator
  float x;              // raw x offset (mm)
  float y;              // raw y offset (mm)
  float xf;             // filtered x offset (mm)
  float yf;             // filtered y offset (mm)
  float offset_mm;      // filtered radial offset from center
  float severity;       // 0.0 = ideal, 1.0 = worst
  float effectiveness;  // 1.0 = full effectiveness, 0.0 = no effective flow
  HandPrompt prompt;
};

struct HandPlacementState {
  bool filter_initialized = false;
  bool correcting = false;
  float xf = 0.0f;
  float yf = 0.0f;
  HandPrompt last_prompt = HandPrompt::OK;
};

void handPlacementInitGeometry();

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
);

const char* promptToString(HandPrompt p);