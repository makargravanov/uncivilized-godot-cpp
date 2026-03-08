//
// Created by Alex on 27.02.2026.
//

#ifndef BIOMECLASSIFIER_H
#define BIOMECLASSIFIER_H

#include <memory>

#include "TileData.h"
#include "elevations-creation/DiscreteLandTypeByHeight.h"

struct ClimateState;

// Converts the old monolithic DiscreteLandTypeByHeight enum into
// the new layered TileData (relief + biome).
class BiomeClassifier {
public:
    static std::unique_ptr<TileData[]> classify(
        const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height);

    static bool classifyFromClimate(
        const ClimateState& climateState,
        TileData* tiles,
        u32 width,
        u32 height);
};

#endif //BIOMECLASSIFIER_H
