//
// Created by Alex on 27.02.2026.
//

#ifndef RELIEFTYPE_H
#define RELIEFTYPE_H

#include "util/declarations.h"

// Geometric + surface classification — determines mesh AND shader group.
// Each value maps to one MultiMesh group (mesh + material).
enum ReliefType : u8 {
    RELIEF_OCEAN         = 0,  // flat hex, water surface (separate shader group)
    RELIEF_LAND          = 1,  // flat hex, land surface
    RELIEF_HILL          = 2,  // hill hex (elevated terrain)
    RELIEF_MOUNTAIN      = 3,  // mountain hex (peaked terrain)
    RELIEF_HIGH_MOUNTAIN = 4,  // placeholder — currently uses MOUNTAIN mesh
    RELIEF_TYPE_COUNT
};

#endif //RELIEFTYPE_H
