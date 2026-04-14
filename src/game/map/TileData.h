//
// Created by Alex on 27.02.2026.
//

#ifndef TILEDATA_H
#define TILEDATA_H

#include "ReliefType.h"
#include "BiomeType.h"
#include "FeatureType.h"

// Per-tile aggregate — one TileData per hex cell on the map.
struct TileData {
    ReliefType   relief;
    BiomeType    biome;
    i8           temperature = 0;  // Published temperature snapshot in Celsius for UI/gameplay.
    u8           humidity    = 0;   // Published humidity snapshot, normalized 0..255.
    u8           river_edges = 0;   // 6 bits — one per hex edge (0=N, clockwise)
    FeatureFlags features    = Feature::NONE;
    BiomeType    pendingBiome = BIOME_TYPE_COUNT; // Delayed climate-driven biome target.
    u16          pendingBiomePersistenceYears = 0;
    f32          cryosphere  = 0.0f; // Combined snow / ice coverage for rendering, 0..1.
};

#endif //TILEDATA_H
