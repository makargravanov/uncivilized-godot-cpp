//
// Created by Alex on 27.06.2025.
//

#ifndef MAPMANAGER_H
#define MAPMANAGER_H
#include "../../util/declarations.h"
#include <memory>


enum DiscreteLandTypeByHeight : u8;
class MapManager {
public:
    std::unique_ptr<DiscreteLandTypeByHeight[]> elevations;

    explicit MapManager(std::unique_ptr<DiscreteLandTypeByHeight[]> elevations) : elevations(std::move(elevations)) {}

    MapManager(const MapManager& other)                = delete;
    MapManager& operator=(const MapManager& other)     = delete;

    MapManager(MapManager&& other) noexcept            = delete;
    MapManager& operator=(MapManager&& other) noexcept = delete;
};



#endif //MAPMANAGER_H
