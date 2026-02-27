//
// Created by Alex on 27.02.2026.
//

#ifndef TILEDATA_H
#define TILEDATA_H

#include "ReliefType.h"
#include "BiomeType.h"

// Per-tile aggregate — one TileData per hex cell on the map.
struct TileData {
    ReliefType relief;
    BiomeType  biome;
    // Future: FeatureType feature = FEATURE_NONE;
    // Future: ClimateData climate;
};

#endif //TILEDATA_H
