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
    BIOME_GRASSLAND          = 4,
    BIOME_HIGHLAND           = 5,
    BIOME_ALPINE             = 6,
    BIOME_TUNDRA             = 7,
    BIOME_BOREAL_FOREST      = 8,
    BIOME_TEMPERATE_FOREST   = 9,
    BIOME_SAVANNA            = 10,
    BIOME_DESERT             = 11,
    BIOME_TROPICAL_FOREST    = 12,
    BIOME_TYPE_COUNT
};

inline bool isWaterBiome(BiomeType b) {
    return b <= BIOME_SHALLOW_INLAND_SEA;
}

inline bool isForestBiome(BiomeType b) {
    return b == BIOME_BOREAL_FOREST
        || b == BIOME_TEMPERATE_FOREST
        || b == BIOME_TROPICAL_FOREST;
}

#endif //BIOMETYPE_H
