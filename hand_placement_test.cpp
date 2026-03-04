// Temporary test harness for CDE2 hand placement logic
// Uses keyboard input over Serial in lieu of physical FSR hardware

#include <Arduino.h>
#include <math.h>

// ==============================
// Hand placement test harness
// Square layout, 8 sensors:
// s1 = 0° (toward head, +y)
// clockwise:
// s2 = 45° (top-right)
// s3 = 90° (right)
// s4 = 135° (bottom-right)
// s5 = 180° (bottom)
// s6 = 225° (bottom-left)
// s7 = 270° (left)
// s8 = 315° (top-left)
// ==============================

// ---------- Tunables ----------
static float r   = 1.0f;    // "radius" to midpoint sensors (units don't matter for sim)
static float tau = 0.15f;   // tolerance band (your variable)
static float S_min = 1.0f;  // minimum total force threshold

// ---------- Simulation state ----------
static float s[8];          // sensor values s1..s8 (stored 0..7)

// Utility
static inline float clamp0(float v) { return (v < 0.0f) ? 0.0f : v; }

enum class Prompt {
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

struct Result {
  float S;
  float Tx;
  float Ty;
  float x;   // normalized
  float y;   // normalized
  Prompt prompt;
};

// Compute Tx, Ty based on your derived expressions, then normalize by S
Result computeHandPlacement(const float s_in[8], float r_in, float tau_in, float Smin) {
  // map to s1..s8
  const float s1 = clamp0(s_in[0]);
  const float s2 = clamp0(s_in[1]);
  const float s3 = clamp0(s_in[2]);
  const float s4 = clamp0(s_in[3]);
  const float s5 = clamp0(s_in[4]);
  const float s6 = clamp0(s_in[5]);
  const float s7 = clamp0(s_in[6]);
  const float s8 = clamp0(s_in[7]);

  Result out{};
  out.S = s1+s2+s3+s4+s5+s6+s7+s8;

  // Your square-geometry numerators
  out.Tx = r_in * ((s2+s3+s4) - (s6+s7+s8));
  out.Ty = r_in * ((s1+s2+s8) - (s4+s5+s6));

  if (out.S < Smin) {
    out.x = 0.0f;
    out.y = 0.0f;
    out.prompt = Prompt::NO_HANDS;
    return out;
  }

  out.x = out.Tx / out.S;
  out.y = out.Ty / out.S;

  const float thr = r_in * tau_in;

  const bool x_hi = (out.x >  thr);
  const bool x_lo = (out.x < -thr);
  const bool y_hi = (out.y >  thr);
  const bool y_lo = (out.y < -thr);

  // Prompts should be opposite the bias direction:
  // if x>0 (pressure biased right), user should move left, etc.
  if (!x_hi && !x_lo && !y_hi && !y_lo) {
    out.prompt = Prompt::OK;
  } else if (y_lo && x_lo) {
    out.prompt = Prompt::MOVE_UP_RIGHT;
  } else if (y_lo && x_hi) {
    out.prompt = Prompt::MOVE_UP_LEFT;
  } else if (y_hi && x_lo) {
    out.prompt = Prompt::MOVE_DOWN_RIGHT;
  } else if (y_hi && x_hi) {
    out.prompt = Prompt::MOVE_DOWN_LEFT;
  } else if (x_hi) {
    out.prompt = Prompt::MOVE_LEFT;
  } else if (x_lo) {
    out.prompt = Prompt::MOVE_RIGHT;
  } else if (y_hi) {
    out.prompt = Prompt::MOVE_DOWN;
  } else { // y_lo
    out.prompt = Prompt::MOVE_UP;
  }

  return out;
}

// ---------------- Simulation helpers ----------------
void setAll(float v) {
  for (int i=0;i<8;i++) s[i]=v;
}

// Add bias by boosting a subset of sensors
void biasUp(float amt) {    // toward head (+y): s1,s2,s8
  s[0]+=amt; s[1]+=amt; s[7]+=amt;
}
void biasDown(float amt) {  // toward feet (-y): s4,s5,s6
  s[3]+=amt; s[4]+=amt; s[5]+=amt;
}
void biasRight(float amt) { // +x: s2,s3,s4
  s[1]+=amt; s[2]+=amt; s[3]+=amt;
}
void biasLeft(float amt) {  // -x: s6,s7,s8
  s[5]+=amt; s[6]+=amt; s[7]+=amt;
}

void printHelp() {
  Serial.println();
  Serial.println("=== Hand Placement Test (8-sensor square) ===");
  Serial.println("Keys:");
  Serial.println("  0 : reset centered (all equal)");
  Serial.println("  w/s : bias force up/down (head/feet)");
  Serial.println("  d/a : bias force right/left");
  Serial.println("  e/q : bias up-right / up-left (diagonal)");
  Serial.println("  c/z : bias down-right / down-left (diagonal)");
  Serial.println("  + / - : increase / decrease tau");
  Serial.println("  [ / ] : decrease / increase total force baseline");
  Serial.println("  h : print this help");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {;}

  setAll(10.0f); // start "centered"
  printHelp();
}

void loop() {
  // --------- Handle serial commands ---------
  if (Serial.available()) {
    char ch = Serial.read();

    // Small step sizes so you can see it gradually
    const float stepBias = 8.0f;
    const float stepForce = 2.0f;

    if (ch == 'h') printHelp();

    if (ch == '0') setAll(10.0f);

    if (ch == 'w') biasUp(stepBias);
    if (ch == 's') biasDown(stepBias);
    if (ch == 'd') biasRight(stepBias);
    if (ch == 'a') biasLeft(stepBias);

    if (ch == 'e') { biasUp(stepBias); biasRight(stepBias); }     // up-right
    if (ch == 'q') { biasUp(stepBias); biasLeft(stepBias); }      // up-left
    if (ch == 'c') { biasDown(stepBias); biasRight(stepBias); }   // down-right
    if (ch == 'z') { biasDown(stepBias); biasLeft(stepBias); }    // down-left

    if (ch == '+') tau += 0.02f;
    if (ch == '-') tau = max(0.0f, tau - 0.02f);

    if (ch == ']') { for(int i=0;i<8;i++) s[i]+=stepForce; }
    if (ch == '[') { for(int i=0;i<8;i++) s[i]=max(0.0f, s[i]-stepForce); }

    Serial.print("tau = "); Serial.println(tau, 3);
  }

  // --------- Compute result ---------
  Result res = computeHandPlacement(s, r, tau, S_min);

  // --------- Print at a readable rate ---------
  static uint32_t lastPrint = 0;
  uint32_t now = millis();
  if (now - lastPrint >= 200) { // 5 Hz print
    lastPrint = now;

    Serial.print("S=");  Serial.print(res.S, 2);
    Serial.print("  Tx="); Serial.print(res.Tx, 2);
    Serial.print("  Ty="); Serial.print(res.Ty, 2);
    Serial.print("  x=");  Serial.print(res.x, 3);
    Serial.print("  y=");  Serial.print(res.y, 3);
    Serial.print("  tau="); Serial.print(tau, 3);
    Serial.print("  -> "); Serial.println(promptToString(res.prompt));
  }
}
