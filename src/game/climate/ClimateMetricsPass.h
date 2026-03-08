#ifndef CLIMATEMETRICSPASS_H
#define CLIMATEMETRICSPASS_H

#include "ClimateState.h"

class ClimateMetricsPass {
public:
    static void initialize(ClimateState& climateState);
    static void advanceOneTurn(ClimateState& climateState);
};

#endif //CLIMATEMETRICSPASS_H