#include "pump_control.h"

// Motor Shield A pins (same as you used before)
constexpr uint8_t PIN_PWM_PUMP  = 3;   // Motor Shield A PWM (goes to pump)
constexpr uint8_t PIN_LED       = 6;   // LED on D6 as visual
constexpr uint8_t PIN_DIR_MOTOR = 12;  // Motor A direction
constexpr uint8_t PIN_BRK_MOTOR = 9;   // Motor A brake

// Map DBP → base PWM (no sputter randomness yet)
static int base_pwm_from_dbp(float dbp_mmHg) {
  if (dbp_mmHg > 34.0f) {
    return PWM_ON;          // good CPR → strong flow
  } else if (dbp_mmHg >= 24.0f) {
    return PWM_SPUTTER;     // marginal → sputter range
  } else {
    return PWM_OFF;         // poor → no flow
  }
}

void pump_init() {
  pinMode(PIN_PWM_PUMP, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_DIR_MOTOR, OUTPUT);
  pinMode(PIN_BRK_MOTOR, OUTPUT);

  // Motor Shield: fixed direction, brake released
  digitalWrite(PIN_DIR_MOTOR, HIGH);  // pick one direction
  digitalWrite(PIN_BRK_MOTOR, LOW);   // LOW = run, HIGH = brake

  analogWrite(PIN_PWM_PUMP, 0);
  analogWrite(PIN_LED, 0);
}

int pump_set_from_dbp(float dbp_mmHg) {
  int pwm = base_pwm_from_dbp(dbp_mmHg);

  // Add sputter randomness only in sputter zone
  if (pwm == PWM_SPUTTER) {
    // Example: 30% ON, 70% OFF → sputter feel
    if (random(0, 100) < 70) {
      pwm = PWM_SPUTTER;
    } else {
      pwm = PWM_OFF;
    }
  }

  // Drive pump + LED
  analogWrite(PIN_PWM_PUMP, pwm);
  analogWrite(PIN_LED, pwm);

  return pwm;
}
