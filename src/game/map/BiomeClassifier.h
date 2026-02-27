//
// Created by Alex on 27.02.2026.
//

#ifndef BIOMECLASSIFIER_H
#define BIOMECLASSIFIER_H

#include <memory>
#include "TileData.h"
#include "elevations-creation/DiscreteLandTypeByHeight.h"

// Converts the old monolithic DiscreteLandTypeByHeight enum into
// the new layered TileData (relief + biome).
// This is a temporary stub — in the future the biome will come from
// a real climate simulation, not from a 1:1 mapping of elevation bands.
class BiomeClassifier {
public:
    static std::unique_ptr<TileData[]> classify(
        const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height)
    {
        const u32 total = width * height;
        auto tiles = std::make_unique<TileData[]>(total);

        for (u32 i = 0; i < total; ++i) {
            switch (discrete[i]) {
                case OCEAN:          tiles[i] = { RELIEF_FLAT,          BIOME_DEEP_OCEAN };         break;
                case SHALLOW_OCEAN:  tiles[i] = { RELIEF_FLAT,          BIOME_SHALLOW_OCEAN };      break;
                case VALLEY:         tiles[i] = { RELIEF_FLAT,          BIOME_INLAND_SEA };         break;
                case SHALLOW_VALLEY: tiles[i] = { RELIEF_FLAT,          BIOME_SHALLOW_INLAND_SEA }; break;
                case PLAIN:          tiles[i] = { RELIEF_FLAT,          BIOME_GRASSLAND };          break;
                case HILL:           tiles[i] = { RELIEF_HILL,          BIOME_HIGHLAND };           break;
                case MOUNTAIN:       tiles[i] = { RELIEF_MOUNTAIN,      BIOME_ALPINE };             break;
                case HIGH_MOUNTAIN:  tiles[i] = { RELIEF_HIGH_MOUNTAIN, BIOME_ALPINE };             break;
                default:             tiles[i] = { RELIEF_FLAT,          BIOME_GRASSLAND };          break;
            }
        }
        return tiles;
    }
};

#endif //BIOMECLASSIFIER_H
