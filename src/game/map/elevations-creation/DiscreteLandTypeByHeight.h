//
// Created by Alex on 27.06.2025.
//

#ifndef DISCRETEHEIGHTLANDTYPE_H
#define DISCRETEHEIGHTLANDTYPE_H
#include "util/declarations.h"
enum DiscreteLandTypeByHeight : u8 {
    OCEAN = 0,
    SHALLOW_OCEAN = 1,
    VALLEY = 2,
    SHALLOW_VALLEY = 3,
    PLAIN = 4,
    HILL = 5,
    MOUNTAIN = 6,
    HIGH_MOUNTAIN = 7,
};
#endif //DISCRETEHEIGHTLANDTYPE_H
