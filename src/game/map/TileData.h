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
    u8           river_edges = 0;   // 6 bits — one per hex edge (0=N, clockwise)
    FeatureFlags features    = Feature::NONE;
    // Future: i8  temperature;
    // Future: u8  precipitation;
};

#endif //TILEDATA_H
