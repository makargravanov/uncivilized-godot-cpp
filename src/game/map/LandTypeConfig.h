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
    const char* materialPath;
};

// One mesh + material per relief group.
// Ocean / land / mountain each get their own shader.
inline constexpr LandTypeConfig LAND_TYPE_CONFIGS[] = {
    {RELIEF_OCEAN,         "res://hexagon_mesh.tres",          "res://shaders/tile_ocean_material.tres"},
    {RELIEF_LAND,          "res://hexagon_mesh.tres",          "res://shaders/tile_land_material.tres"},
    {RELIEF_HILL,          "res://hill_hexagon_mesh.tres",     "res://shaders/tile_land_material.tres"},
    {RELIEF_MOUNTAIN,      "res://mountain_hexagon_mesh.tres", "res://shaders/tile_mountain_material.tres"},
    {RELIEF_HIGH_MOUNTAIN, "res://mountain_hexagon_mesh.tres", "res://shaders/tile_mountain_material.tres"},
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

inline const char* getMaterialPath(const ReliefType type) {
    for (auto& config : LAND_TYPE_CONFIGS) {
        if (config.type == type) {
            return config.materialPath;
        }
    }
    return "res://shaders/tile_land_material.tres";
}

#endif //LANDTYPECONFIG_H
