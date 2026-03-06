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
    i8           temperature = 0;   // °C, range −128..+127
    u16          precipitation = 0; // mm/year, 0–65535
};

#endif //TILEDATA_H
