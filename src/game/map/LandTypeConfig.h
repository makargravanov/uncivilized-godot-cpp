//
// Created by Alex on 23.01.2026.
//

#ifndef LANDTYPECONFIG_H
#define LANDTYPECONFIG_H

#include "elevations-creation/DiscreteLandTypeByHeight.h"
#include <array>

struct LandTypeConfig {
    DiscreteLandTypeByHeight type;
    const char* meshPath;
};

inline constexpr LandTypeConfig LAND_TYPE_CONFIGS[] = {
    {OCEAN,          "res://ocean_hexagon_mesh.tres"},
    {SHALLOW_OCEAN,  "res://shallow_ocean_hexagon_mesh.tres"},
    {VALLEY,         "res://valley_hexagon_mesh.tres"},
    {SHALLOW_VALLEY, "res://shallow_valley_hexagon_mesh.tres"},
    {PLAIN,          "res://hexagon_mesh.tres"},
    {HILL,           "res://hill_hexagon_mesh.tres"},
    {MOUNTAIN,       "res://mountain_hexagon_mesh.tres"},
    {HIGH_MOUNTAIN,  "res://high_mountain_hexagon_mesh.tres"},
};

inline constexpr size_t LAND_TYPE_COUNT = std::size(LAND_TYPE_CONFIGS);
static_assert(LAND_TYPE_COUNT == HIGH_MOUNTAIN + 1,
    "Not all land types have mesh configs!");

inline const char* getMeshPath(const DiscreteLandTypeByHeight type) {
    for (auto& config : LAND_TYPE_CONFIGS) {
        if (config.type == type) {
            return config.meshPath;
        }
    }
    return "res://hexagon_mesh.tres";
}


#endif //LANDTYPECONFIG_H
