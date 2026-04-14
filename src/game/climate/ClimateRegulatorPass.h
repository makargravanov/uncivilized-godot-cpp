#ifndef CLIMATEREGULATORPASS_H
#define CLIMATEREGULATORPASS_H

#include "ClimateState.h"

class ClimateRegulatorPass {
public:
    static void initialize(ClimateState& climateState);
    static void advanceOneTurn(ClimateState& climateState);
};

#endif //CLIMATEREGULATORPASS_H
