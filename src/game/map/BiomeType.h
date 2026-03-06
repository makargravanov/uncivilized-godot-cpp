//
// Created by Alex on 27.02.2026.
//

#ifndef BIOMETYPE_H
#define BIOMETYPE_H

#include "util/declarations.h"

// Visual / ecological classification — determines colour and future gameplay properties.
// Packed into INSTANCE_CUSTOM.x and read by the tile shaders.
// Water biomes 0–3, land biomes 4+.
enum BiomeType : u8 {
    // ── Water ──
    BIOME_DEEP_OCEAN          = 0,
    BIOME_SHALLOW_OCEAN       = 1,
    BIOME_INLAND_SEA          = 2,
    BIOME_SHALLOW_INLAND_SEA  = 3,

    // ── Tropical (T > 20 °C) ──
    BIOME_TROPICAL_RAINFOREST = 4,
    BIOME_TROPICAL_SEASONAL   = 5,
    BIOME_SAVANNA             = 6,
    BIOME_SUBTROPICAL_DESERT  = 7,

    // ── Temperate (5–20 °C) ──
    BIOME_TEMPERATE_FOREST    = 8,
    BIOME_TEMPERATE_GRASSLAND = 9,
    BIOME_TEMPERATE_DESERT    = 10,

    // ── Boreal & Polar ──
    BIOME_BOREAL_FOREST       = 11,  // Taiga
    BIOME_TUNDRA              = 12,
    BIOME_ICE_SHEET           = 13,

    // ── Altitude-driven ──
    BIOME_HIGHLAND            = 14,
    BIOME_ALPINE              = 15,

    BIOME_TYPE_COUNT
};

inline bool isWaterBiome(BiomeType b) {
    return b <= BIOME_SHALLOW_INLAND_SEA;
}

#endif //BIOMETYPE_H
