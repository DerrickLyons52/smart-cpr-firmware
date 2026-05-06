#include "pump_control.h"

// Motor Shield A pins
constexpr uint8_t PIN_PWM_PUMP  = 3;   // Motor Shield A PWM
constexpr uint8_t PIN_LED       = 6;   // Optional LED indicator
constexpr uint8_t PIN_DIR_MOTOR = 12;  // Motor A direction
constexpr uint8_t PIN_BRK_MOTOR = 9;   // Motor A brake

void pump_init() {
  pinMode(PIN_PWM_PUMP, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_DIR_MOTOR, OUTPUT);
  pinMode(PIN_BRK_MOTOR, OUTPUT);

  // Fixed direction, brake released
  digitalWrite(PIN_DIR_MOTOR, HIGH);
  digitalWrite(PIN_BRK_MOTOR, LOW);

  analogWrite(PIN_PWM_PUMP, PWM_OFF);
  analogWrite(PIN_LED, PWM_OFF);
}

int pump_set_binary(bool flow_on) {
  int pwm = flow_on ? PWM_ON : PWM_OFF;

  analogWrite(PIN_PWM_PUMP, pwm);
  analogWrite(PIN_LED, pwm);

  return pwm;
}