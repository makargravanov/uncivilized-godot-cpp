//
// Created by Alex on 23.01.2026.
//

#ifndef LANDTYPECONFIG_H
#define LANDTYPECONFIG_H

#include "ReliefType.h"
#include <array>

struct LandTypeConfig {
    ReliefType type;
    const char* meshPath;
};

// One mesh per relief geometry.  HIGH_MOUNTAIN reuses the MOUNTAIN mesh.
inline constexpr LandTypeConfig LAND_TYPE_CONFIGS[] = {
    {RELIEF_FLAT,          "res://hexagon_mesh.tres"},
    {RELIEF_HILL,          "res://hill_hexagon_mesh.tres"},
    {RELIEF_MOUNTAIN,      "res://mountain_hexagon_mesh.tres"},
    {RELIEF_HIGH_MOUNTAIN, "res://mountain_hexagon_mesh.tres"},
};

inline constexpr size_t LAND_TYPE_COUNT = std::size(LAND_TYPE_CONFIGS);
static_assert(LAND_TYPE_COUNT == RELIEF_TYPE_COUNT,
    "Not all relief types have mesh configs!");

inline const char* getMeshPath(const ReliefType type) {
    for (auto& config : LAND_TYPE_CONFIGS) {
        if (config.type == type) {
            return config.meshPath;
        }
    }
    return "res://hexagon_mesh.tres";
}

#endif //LANDTYPECONFIG_H
