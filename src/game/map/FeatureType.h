//
// Created by Alex on 27.02.2026.
//

#ifndef FEATURETYPE_H
#define FEATURETYPE_H

#include "util/declarations.h"

// Tile features — bitflags, multiple can coexist on one tile.
// A tile can have forest AND river AND swamp simultaneously.
using FeatureFlags = u8;

namespace Feature {
    constexpr FeatureFlags NONE           = 0;
    constexpr FeatureFlags HAS_FOREST     = 1 << 0;  // forest present (type determined by biome)
    constexpr FeatureFlags FOREST_CLEARED = 1 << 1;  // forest was cut — potential for regrowth
    constexpr FeatureFlags HAS_SWAMP      = 1 << 2;
    constexpr FeatureFlags FLOOD_PLAIN    = 1 << 3;  // river floodplain
    constexpr FeatureFlags HAS_ICE        = 1 << 4;  // snow or surface ice cover (climate-driven)
    // 3 bits free
}

#endif //FEATURETYPE_H
