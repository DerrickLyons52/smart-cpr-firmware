#pragma once
#include <Arduino.h>

// Discrete pump states mapped to PWM levels
constexpr int PWM_OFF      = 0;    // pump off
constexpr int PWM_SPUTTER  = 120;  // medium
constexpr int PWM_ON       = 200;  // strong

// Call once in setup()
void pump_init();

// Call every loop after you compute DBP
// - dbp_mmHg: final DBP after penalties
// Returns the actual PWM written (for logging)
int pump_set_from_dbp(float dbp_mmHg);
