#ifndef DBP_H
#define DBP_H

#include <Arduino.h>

struct CprMetrics {
    float relative_depth_mm;
    float lean_mm;
    float rate_cpm;
    float pause_scale;   // 1.0 = no penalty, 0.67 = full pause penalty
};

float dbp_from_depth(float relative_depth_mm);
float rate_scale(float rate_cpm);
float lean_scale(float lean_mm);

float compute_dbp_mmHg(const CprMetrics &m);

#endif