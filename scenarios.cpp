#include "scenarios.h"
#include "dbp.h"

CprMetrics scenario_good = {
    50.0f,   // depth
    0.0f,    // lean
    110.0f,  // rate
    0.0f,    // placement
    false
};

CprMetrics scenario_okay = {
    32.0f,  // depth
    2.0f,   // lean
    110.0f, // rate
    0.0f,   // placement
    false
};

CprMetrics scenario_bad = {
    10.0f,  // depth
    5.0f,   // lean
    60.0f,  // rate
    2.0f,   // placement
    false
};

CprMetrics scenario_cycle = scenario_good;

void update_cycle_scenario(unsigned long ms) {
    unsigned long t = (ms / 3000) % 3;

    if (t == 0)      scenario_cycle = scenario_good;
    else if (t == 1) scenario_cycle = scenario_okay;
    else             scenario_cycle = scenario_bad;
}
