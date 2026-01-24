#ifndef SCENARIOS_H
#define SCENARIOS_H

#include "dbp.h"

extern CprMetrics scenario_good;
extern CprMetrics scenario_okay;
extern CprMetrics scenario_bad;
extern CprMetrics scenario_cycle;  // special cycling one

// update cycle-scenario based on time
void update_cycle_scenario(unsigned long ms);

#endif
