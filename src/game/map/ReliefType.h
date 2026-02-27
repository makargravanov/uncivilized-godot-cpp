//
// Created by Alex on 27.02.2026.
//

#ifndef RELIEFTYPE_H
#define RELIEFTYPE_H

#include "util/declarations.h"

// Purely geometric classification — determines which mesh to render.
// Each value maps to exactly one mesh geometry.
enum ReliefType : u8 {
    RELIEF_FLAT          = 0,  // flat hex (ocean, plain, depression, lowland — all flat geometry)
    RELIEF_HILL          = 1,  // hill hex (elevated terrain)
    RELIEF_MOUNTAIN      = 2,  // mountain hex (peaked terrain)
    RELIEF_HIGH_MOUNTAIN = 3,  // placeholder — currently uses MOUNTAIN mesh
    RELIEF_TYPE_COUNT
};

#endif //RELIEFTYPE_H
