//
// Created by Copilot on 06.03.2026.
//

#ifndef WINDPASS_H
#define WINDPASS_H

#include "ClimateState.h"

class WindPass {
public:
    static void initialize(ClimateState& climateState);
    static void advanceOneTurn(ClimateState& climateState);
};

#endif //WINDPASS_H