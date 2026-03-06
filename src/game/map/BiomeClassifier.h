//
// Created by Alex on 27.02.2026.
//

#ifndef BIOMECLASSIFIER_H
#define BIOMECLASSIFIER_H

#include <memory>
#include "TileData.h"
#include "elevations-creation/DiscreteLandTypeByHeight.h"

class ClimateModel;

// Converts DiscreteLandTypeByHeight → TileData with relief assignment,
// then classifies land biomes from climate data (Whittaker diagram).
class BiomeClassifier {
public:
    // Phase 1: build TileData from discrete elevation classification (no climate yet).
    static std::unique_ptr<TileData[]> classify(
        const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height)
    {
        const u32 total = width * height;
        auto tiles = std::make_unique<TileData[]>(total);

        for (u32 i = 0; i < total; ++i) {
            switch (discrete[i]) {
                case OCEAN:          tiles[i] = { RELIEF_OCEAN,         BIOME_DEEP_OCEAN };          break;
                case SHALLOW_OCEAN:  tiles[i] = { RELIEF_OCEAN,         BIOME_SHALLOW_OCEAN };       break;
                case VALLEY:         tiles[i] = { RELIEF_OCEAN,         BIOME_INLAND_SEA };          break;
                case SHALLOW_VALLEY: tiles[i] = { RELIEF_OCEAN,         BIOME_SHALLOW_INLAND_SEA };  break;
                case PLAIN:          tiles[i] = { RELIEF_LAND,          BIOME_TEMPERATE_GRASSLAND }; break;
                case HILL:           tiles[i] = { RELIEF_HILL,          BIOME_HIGHLAND };            break;
                case MOUNTAIN:       tiles[i] = { RELIEF_MOUNTAIN,      BIOME_ALPINE };              break;
                case HIGH_MOUNTAIN:  tiles[i] = { RELIEF_HIGH_MOUNTAIN, BIOME_ALPINE };              break;
                default:             tiles[i] = { RELIEF_LAND,          BIOME_TEMPERATE_GRASSLAND }; break;
            }
        }
        return tiles;
    }

    // Phase 2: reclassify land biomes using climate model output (Whittaker diagram).
    // Call after ClimateModel::warmup() has completed.
    static void reclassifyFromClimate(TileData* tiles, const ClimateModel& climate, u32 total);

    // Single-tile Whittaker classification from temperature (°C) and precipitation (mm/month).
    static BiomeType whittaker(f32 temp_c, f32 precip_mm, f32 elevation_m, ReliefType relief);
};

#endif //BIOMECLASSIFIER_H
