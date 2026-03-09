//
// Created by Alex on 27.02.2026.
//

#ifndef BIOMETYPE_H
#define BIOMETYPE_H

#include "util/declarations.h"

// Visual / ecological classification — determines colour and future gameplay properties.
// Packed into INSTANCE_CUSTOM.x and read by the tile_biome shader.
enum BiomeType : u8 {
    BIOME_DEEP_OCEAN         = 0,
    BIOME_SHALLOW_OCEAN      = 1,
    BIOME_INLAND_SEA         = 2,
    BIOME_SHALLOW_INLAND_SEA = 3,
    BIOME_POLAR_DESERT       = 4,
    BIOME_TUNDRA             = 5,
    BIOME_BOREAL_FOREST      = 6,
    BIOME_COLD_STEPPE        = 7,
    BIOME_TEMPERATE_FOREST   = 8,
    BIOME_TEMPERATE_STEPPE   = 9,
    BIOME_XERIC_SHRUBLAND    = 10,
    BIOME_HOT_DESERT         = 11,
    BIOME_SAVANNA            = 12,
    BIOME_TROPICAL_SEASONAL_FOREST = 13,
    BIOME_TROPICAL_RAINFOREST = 14,
    BIOME_ALPINE             = 15,
    BIOME_TYPE_COUNT
};

inline bool isWaterBiome(BiomeType b) {
    return b <= BIOME_SHALLOW_INLAND_SEA;
}

inline bool isForestCapableBiome(BiomeType b) {
    return b == BIOME_BOREAL_FOREST
        || b == BIOME_TEMPERATE_FOREST
    || b == BIOME_TROPICAL_SEASONAL_FOREST
    || b == BIOME_TROPICAL_RAINFOREST;
}

#endif //BIOMETYPE_H
