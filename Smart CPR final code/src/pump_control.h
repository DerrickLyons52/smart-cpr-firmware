#pragma once
#include <Arduino.h>

constexpr int PWM_OFF = 0;
constexpr int PWM_ON  = 200;

// Call once in setup()
void pump_init();

// Binary pump control for demo
// flow_on = true  -> pump ON
// flow_on = false -> pump OFF
// Returns actual PWM written
int pump_set_binary(bool flow_on);