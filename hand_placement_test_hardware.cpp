#include <Arduino.h>
#include <math.h>

// ===== Hand Placement Hardware Test (4 Pots at corners) =====
// 45°  -> s2 (top-right)   -> A0
// 135° -> s4 (bottom-right)-> A1
// 225° -> s6 (bottom-left) -> A2
// 315° -> s8 (top-left)    -> A3
//
// Pipeline:
// 1) Read pots -> s2,s4,s6,s8
// 2) Compute Tx,Ty (raw imbalance)
// 3) Normalize: x=Tx/S, y=Ty/S
// 4) Low-pass filter: xf,yf (EMA)
// 5) Hysteresis: enter/exit thresholds on |xf|,|yf|
// 6) Direction prompt (incl diagonals) using xf,yf

// --- Geometry scale (keep as 1.0 for prototype) ---
static float r = 1.0f;

// --- Force gate to ignore noise / "no hands" ---
static float S_min = 20.0f;   // tune later

// --- Low-pass filter strength (0<alpha<=1) ---
static float alpha = 0.20f;   // 0.2 is a good start

// --- Hysteresis thresholds (enter > exit) ---
static float tau_enter = 0.15f;  // must exceed this to START prompting
static float tau_exit  = 0.10f;  // must go below this to return to OK

// --- Analog pin mapping ---
static const int PIN_45  = A0; // s2
static const int PIN_135 = A1; // s4
static const int PIN_225 = A2; // s6
static const int PIN_315 = A3; // s8

enum class Prompt {
  OK,
  MOVE_LEFT, MOVE_RIGHT, MOVE_UP, MOVE_DOWN,
  MOVE_UP_LEFT, MOVE_UP_RIGHT, MOVE_DOWN_LEFT, MOVE_DOWN_RIGHT,
  NO_HANDS
};

const char* promptToString(Prompt p) {
  switch (p) {
    case Prompt::OK: return "OK (good placement)";
    case Prompt::MOVE_LEFT: return "Move LEFT";
    case Prompt::MOVE_RIGHT: return "Move RIGHT";
    case Prompt::MOVE_UP: return "Move UP (toward head)";
    case Prompt::MOVE_DOWN: return "Move DOWN (toward feet)";
    case Prompt::MOVE_UP_LEFT: return "Move UP-LEFT";
    case Prompt::MOVE_UP_RIGHT: return "Move UP-RIGHT";
    case Prompt::MOVE_DOWN_LEFT: return "Move DOWN-LEFT";
    case Prompt::MOVE_DOWN_RIGHT: return "Move DOWN-RIGHT";
    case Prompt::NO_HANDS: return "No hands / insufficient force";
    default: return "Unknown";
  }
}

// Classifier with diagonals (IMPORTANT: prompt is opposite direction of bias):
// - If xf > 0 (more force on right), user should move LEFT.
static Prompt classifyPrompt(float xf, float yf, float thr) {
  const bool x_hi = (xf >  thr);
  const bool x_lo = (xf < -thr);
  const bool y_hi = (yf >  thr);
  const bool y_lo = (yf < -thr);

  if (!x_hi && !x_lo && !y_hi && !y_lo) return Prompt::OK;

  // Diagonals first
  if (y_lo && x_lo) return Prompt::MOVE_UP_RIGHT;
  if (y_lo && x_hi) return Prompt::MOVE_UP_LEFT;
  if (y_hi && x_lo) return Prompt::MOVE_DOWN_RIGHT;
  if (y_hi && x_hi) return Prompt::MOVE_DOWN_LEFT;

  // Cardinals
  if (x_hi) return Prompt::MOVE_LEFT;
  if (x_lo) return Prompt::MOVE_RIGHT;
  if (y_hi) return Prompt::MOVE_DOWN;
  return Prompt::MOVE_UP; // y_lo
}

// --- Filter state (persists across loop iterations) ---
static bool filter_initialized = false;
static float xf = 0.0f;
static float yf = 0.0f;

// --- Hysteresis state ---
static bool correcting = false;
static Prompt latchedPrompt = Prompt::OK;

void setup() {
  Serial.begin(115200);
  while (!Serial) {;}
  Serial.println("=== Hand Placement Test: Filter + Hysteresis + Diagonals (4 pots) ===");
  Serial.println("Pins: s2=A0, s4=A1, s6=A2, s8=A3");
}

void loop() {
  // 1) Read pots (raw)
  float s2 = analogRead(PIN_45);
  float s4 = analogRead(PIN_135);
  float s6 = analogRead(PIN_225);
  float s8 = analogRead(PIN_315);

  // 2) Total force across active sensors
  float S = s2 + s4 + s6 + s8;

  // Gate: no hands / too little force
  if (S < S_min) {
    correcting = false;
    latchedPrompt = Prompt::NO_HANDS;
    filter_initialized = false; // reset filter so it doesn't "remember" old offsets

    Serial.print("S="); Serial.print(S, 1);
    Serial.println(" -> NO HANDS");
    delay(200);
    return;
  }

  // 3) Raw imbalance using reduced 8-sensor equations with missing sensors = 0:
  // Tx = r * ((s2+s3+s4) - (s6+s7+s8)) -> here s3,s7=0
  // Ty = r * ((s1+s2+s8) - (s4+s5+s6)) -> here s1,s5=0
  float Tx = r * ((s2 + s4) - (s6 + s8));
  float Ty = r * ((s2 + s8) - (s4 + s6));

  // 4) Normalize (force invariance)
  float x = Tx / S;
  float y = Ty / S;

  // 5) Digital low-pass filter (EMA) on x,y -> xf,yf
  if (!filter_initialized) {
    xf = x;
    yf = y;
    filter_initialized = true;
  } else {
    xf = alpha * x + (1.0f - alpha) * xf;
    yf = alpha * y + (1.0f - alpha) * yf;
  }

  // 6) Hysteresis (enter/exit) using filtered values
  float thr_enter = r * tau_enter;
  float thr_exit  = r * tau_exit;

  float ax = fabsf(xf);
  float ay = fabsf(yf);

  bool outside_enter = (ax > thr_enter) || (ay > thr_enter);
  bool inside_exit   = (ax < thr_exit)  && (ay < thr_exit);

  if (!correcting) {
    // Currently OK; only enter correction mode if outside ENTER threshold
    if (outside_enter) {
      correcting = true;
      latchedPrompt = classifyPrompt(xf, yf, thr_enter);
    } else {
      latchedPrompt = Prompt::OK;
    }
  } else {
    // Currently correcting; only exit if fully inside EXIT threshold
    if (inside_exit) {
      correcting = false;
      latchedPrompt = Prompt::OK;
    } else if (outside_enter) {
      // Far outside again -> update direction
      latchedPrompt = classifyPrompt(xf, yf, thr_enter);
    }
    // else: hysteresis band -> hold latchedPrompt to prevent flicker
  }

  // Print debug
  Serial.print("s2="); Serial.print(s2, 0);
  Serial.print(" s4="); Serial.print(s4, 0);
  Serial.print(" s6="); Serial.print(s6, 0);
  Serial.print(" s8="); Serial.print(s8, 0);

  Serial.print(" | S=");  Serial.print(S, 0);
  Serial.print(" Tx=");   Serial.print(Tx, 2);
  Serial.print(" Ty=");   Serial.print(Ty, 2);

  Serial.print(" | x=");  Serial.print(x, 3);
  Serial.print(" y=");    Serial.print(y, 3);
  Serial.print(" | xf="); Serial.print(xf, 3);
  Serial.print(" yf=");   Serial.print(yf, 3);

  Serial.print(" | mode=");
  Serial.print(correcting ? "CORRECT" : "OK");
  Serial.print(" -> ");
  Serial.println(promptToString(latchedPrompt));

  delay(200);
}