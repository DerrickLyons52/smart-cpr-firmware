/*
 * Smart CPR Training System – Firmware
 * ENGR 498 Senior Design
 *
 * This firmware computes CPR quality metrics and drives
 * a pump-based blood flow visualization system.
 *
 * MVP State: Scenario-driven DBP (no live sensors yet)
 */


#include <Arduino.h>
#include <math.h>
#include "dbp.h"
#include "scenarios.h"

#include <Arduino.h>
#include <math.h>
#include "dbp.h"
#include "scenarios.h"

// ----- Pin map -----
constexpr uint8_t PIN_PWM_PUMP  = 3;   // Motor Shield channel A PWM (PWMA)
constexpr uint8_t PIN_LED       = 6;   // Separate LED on D6 (through resistor to GND)
constexpr uint8_t PIN_DIR_MOTOR = 12;  // Motor A direction
constexpr uint8_t PIN_BRK_MOTOR = 9;   // Motor A brake
constexpr uint16_t LOOP_MS      = 10;  // ~100 Hz loop

// sputter timing (tweak these if needed)
constexpr uint16_t SPUTTER_ON_MS  = 400; // how long pump is ON in sputter (ms)
constexpr uint16_t SPUTTER_OFF_MS = 800; // how long pump is OFF in sputter (ms)

// ----- PWM levels for pump state -----
constexpr int PWM_OFF      = 0;
constexpr int PWM_SPUTTER  = 120;
constexpr int PWM_ON       = 200;


int pwm_from_dbp(float dbp) {
    if (dbp > 34)  return PWM_ON;
    if (dbp > 24)  return PWM_SPUTTER;
    return PWM_OFF;
}

int currentScenario = 1;  // start at scenario 1

void setup() {
    Serial.begin(115200);

    pinMode(PIN_PWM_PUMP, OUTPUT);
    pinMode(PIN_DIR_MOTOR, OUTPUT);
    pinMode(PIN_BRK_MOTOR, OUTPUT);

    digitalWrite(PIN_DIR_MOTOR, HIGH);   // choose direction
    digitalWrite(PIN_BRK_MOTOR, LOW);    // release brake (IMPORTANT)

    analogWrite(PIN_PWM_PUMP, 0);        // start off
}


void loop() {
    static unsigned long t0 = millis();
    unsigned long ms = millis() - t0;

    // --- 1. Read keyboard for scenario switching ---
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch == '1') currentScenario = 1;
        if (ch == '2') currentScenario = 2;
        if (ch == '3') currentScenario = 3;
        if (ch == '4') currentScenario = 4;
    }

    // --- 2. Pick CPR metrics based on scenario ---
    CprMetrics m;
    if (currentScenario == 1)      m = scenario_good;
    else if (currentScenario == 2) m = scenario_okay;
    else if (currentScenario == 3) m = scenario_bad;
    else {                          // 4
        update_cycle_scenario(ms);
        m = scenario_cycle;
    }

    // --- 3. Compute DBP ---
    float dbp = compute_dbp_mmHg(m);

    // --- 4. Map DBP → PWM ---
    int pwm = pwm_from_dbp(dbp);

    // sputter pattern: slow ON/OFF pulses so water actually moves
if (pwm == PWM_SPUTTER) {
    static bool sputterOn = false;
    static uint32_t sputter_t0 = 0;

    uint32_t now_ms = ms;  // we already computed ms earlier

    uint16_t interval = sputterOn ? SPUTTER_ON_MS : SPUTTER_OFF_MS;

    if (now_ms - sputter_t0 >= interval) {
        sputterOn = !sputterOn;      // flip state
        sputter_t0 = now_ms;         // restart timer
    }

    pwm = sputterOn ? PWM_SPUTTER : PWM_OFF;
}


    // --- 5. Output ---
    analogWrite(PIN_PWM_PUMP, pwm);
    analogWrite(PIN_LED, pwm);

    /*Serial.print(ms); Serial.print(',');
    Serial.print(currentScenario); Serial.print(',');
    Serial.print(dbp, 2); Serial.print(',');
    Serial.println(pwm);*/

    static int last_scenario = -1;

    if (currentScenario != last_scenario) {
      if (currentScenario == 1)
        Serial.println("Current scenario: always good CPR. ");
      else if (currentScenario == 2)
        Serial.println("Current scenario: always okay CPR. ");
      else if (currentScenario == 3)
        Serial.println("Current scenario: always bad CPR. ");
      else
        Serial.println("Current scenario: inconsistent CPR. ");
      last_scenario = currentScenario;
    }

    delay(LOOP_MS);
}