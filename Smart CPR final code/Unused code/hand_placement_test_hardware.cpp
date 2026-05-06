#include <Arduino.h>
#include <math.h>

static const int NUM_SENSORS = 8;

// ---------- Analog pin mapping ----------
static const int sensorPins[NUM_SENSORS] = {
  A8, A9, A10, A11, A12, A13, A14, A15
};

// ---------- Measured geometry ----------
static const float r_mm[NUM_SENSORS] = {
  30.0f, 30.96f, 31.24f, 29.85f, 30.0f, 30.25f, 30.28f, 30.93f
};

static const float theta_deg[NUM_SENSORS] = {
  0.0f, 45.0f, 89.0f, 136.0f, 180.0f, 225.0f, 269.0f, 307.0f
};

// ---------- Baseline calibration ----------
static float baseline[NUM_SENSORS];
static const int BASELINE_SAMPLES = 150;

// ---------- Coordinates ----------
static float x_pos[NUM_SENSORS];
static float y_pos[NUM_SENSORS];

// ---------- Thresholds ----------
static float SENSOR_DEADBAND = 5.0f;
static float S_enter = 30.0f;
static float S_exit  = 15.0f;

// ---------- Timing ----------
static const unsigned long NO_HANDS_DELAY = 1200;
static const unsigned long DIRECTION_DELAY = 300;
static unsigned long lastHandsTime = 0;
static unsigned long lastDirectionChangeTime = 0;

// ---------- Filter ----------
static float alpha = 0.20f;

// ---------- Direction thresholds ----------
static float tau_enter = 4.0f;
static float tau_exit  = 2.5f;

enum class Prompt {
  OK,
  MOVE_LEFT, MOVE_RIGHT, MOVE_UP, MOVE_DOWN,
  MOVE_UP_LEFT, MOVE_UP_RIGHT, MOVE_DOWN_LEFT, MOVE_DOWN_RIGHT,
  NO_HANDS
};

static bool hasHands = false;
static bool correcting = false;
static bool filter_initialized = false;
static float xf = 0.0f;
static float yf = 0.0f;
static Prompt latchedPrompt = Prompt::NO_HANDS;

static float degToRad(float deg) {
  return deg * (PI / 180.0f);
}

void setupGeometry() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    float theta = degToRad(theta_deg[i]);

    // 0° at top, clockwise positive
    x_pos[i] = r_mm[i] * sinf(theta);
    y_pos[i] = r_mm[i] * cosf(theta);
  }
}

void calibrateBaseline() {
  Serial.println("Calibrating baselines... keep all sensors untouched.");

  for (int i = 0; i < NUM_SENSORS; i++) {
    baseline[i] = 0.0f;
  }

  delay(3000);

  for (int n = 0; n < BASELINE_SAMPLES; n++) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      baseline[i] += analogRead(sensorPins[i]);
    }
    delay(10);
  }

  for (int i = 0; i < NUM_SENSORS; i++) {
    baseline[i] /= BASELINE_SAMPLES;
  }

  Serial.println("Baseline values:");
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print("B"); Serial.print(i);
    Serial.print("=");
    Serial.print(baseline[i], 0);
    Serial.print("  ");
  }
  Serial.println();
  Serial.println("Baseline calibration complete.\n");
}

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

static Prompt classifyPrompt(float xf, float yf, float thr) {
  const bool x_hi = (xf >  thr);
  const bool x_lo = (xf < -thr);
  const bool y_hi = (yf >  thr);
  const bool y_lo = (yf < -thr);

  if (!x_hi && !x_lo && !y_hi && !y_lo) return Prompt::OK;

  if (y_lo && x_lo) return Prompt::MOVE_UP_RIGHT;
  if (y_lo && x_hi) return Prompt::MOVE_UP_LEFT;
  if (y_hi && x_lo) return Prompt::MOVE_DOWN_RIGHT;
  if (y_hi && x_hi) return Prompt::MOVE_DOWN_LEFT;

  if (x_hi) return Prompt::MOVE_LEFT;
  if (x_lo) return Prompt::MOVE_RIGHT;
  if (y_hi) return Prompt::MOVE_DOWN;
  return Prompt::MOVE_UP;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  setupGeometry();
  calibrateBaseline();

  Serial.println("=== Hand Placement Direction Test ===");
}

void loop() {
  float raw[NUM_SENSORS];
  float act[NUM_SENSORS];
  float S = 0.0f;
  float Tx = 0.0f;
  float Ty = 0.0f;

  // Read all sensors
  for (int i = 0; i < NUM_SENSORS; i++) {
    raw[i] = analogRead(sensorPins[i]);
    act[i] = raw[i] - baseline[i];

    if (act[i] < 0.0f) {
      act[i] = 0.0f;
    }

    if (act[i] < SENSOR_DEADBAND) {
      act[i] = 0.0f;
    }

    S += act[i];
    Tx += act[i] * x_pos[i];
    Ty += act[i] * y_pos[i];
  }

  unsigned long now = millis();

  // Hands / no-hands hysteresis
  if (!hasHands && S >= S_enter) {
    hasHands = true;
    lastHandsTime = now;
    filter_initialized = false;
  }

  if (hasHands) {
    if (S >= S_exit) {
      lastHandsTime = now;
    } else if ((now - lastHandsTime) > NO_HANDS_DELAY) {
      hasHands = false;
      correcting = false;
      latchedPrompt = Prompt::NO_HANDS;
      filter_initialized = false;
    }
  }

  float x = 0.0f;
  float y = 0.0f;

  if (hasHands && S > 1.0f) {
    x = Tx / S;
    y = Ty / S;
  }

  // Filter only when hands are present
  if (hasHands) {
    if (!filter_initialized) {
      xf = x;
      yf = y;
      filter_initialized = true;
    } else {
      xf = alpha * x + (1.0f - alpha) * xf;
      yf = alpha * y + (1.0f - alpha) * yf;
    }

    float ax = fabsf(xf);
    float ay = fabsf(yf);

    bool outside_enter = (ax > tau_enter) || (ay > tau_enter);
    bool inside_exit   = (ax < tau_exit) && (ay < tau_exit);

    if (!correcting) {
      if (outside_enter) {
        if ((now - lastDirectionChangeTime) > DIRECTION_DELAY) {
          correcting = true;
          latchedPrompt = classifyPrompt(xf, yf, tau_enter);
          lastDirectionChangeTime = now;
        }
      } else {
        latchedPrompt = Prompt::OK;
      }
    } else {
      if (inside_exit) {
        correcting = false;
        latchedPrompt = Prompt::OK;
      } else if (outside_enter) {
        if ((now - lastDirectionChangeTime) > DIRECTION_DELAY) {
          latchedPrompt = classifyPrompt(xf, yf, tau_enter);
          lastDirectionChangeTime = now;
        }
      }
    }
  } else {
    x = 0.0f;
    y = 0.0f;
    xf = 0.0f;
    yf = 0.0f;
    latchedPrompt = Prompt::NO_HANDS;
  }

  // Debug print
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print("S"); Serial.print(i);
    Serial.print(" raw="); Serial.print(raw[i], 0);
    Serial.print(" act="); Serial.print(act[i], 0);
    Serial.print("  ");
  }

  Serial.print("| Sum=");
  Serial.print(S, 1);
  Serial.print(" | x=");
  Serial.print(x, 2);
  Serial.print(" y=");
  Serial.print(y, 2);
  Serial.print(" | xf=");
  Serial.print(xf, 2);
  Serial.print(" yf=");
  Serial.print(yf, 2);
  Serial.print(" | state=");
  Serial.print(hasHands ? "HANDS" : "NO_HANDS");
  Serial.print(" | mode=");
  Serial.print(correcting ? "CORRECT" : "OK");
  Serial.print(" -> ");
  Serial.println(promptToString(latchedPrompt));

  delay(200);
}