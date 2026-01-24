#ifndef DBP_H
#define DBP_H

#include <Arduino.h>

// CPR metrics struct
struct CprMetrics {
    float depth_mm;
    float lean_mm;
    float rate_cpm;
    float hand_placement_mm;
    bool  paused;
};

// Function declarations (prototypes)
float dbp_from_depth(float depth_mm);
float rate_scale(float rate_cpm);
float lean_scale(float lean_mm);
float placement_scale(float hand_placement_mm);

float compute_dbp_mmHg(const CprMetrics &m);

#endif
