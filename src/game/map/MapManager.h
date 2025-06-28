//
// Created by Alex on 27.06.2025.
//

#ifndef MAPMANAGER_H
#define MAPMANAGER_H
#include "../../util/declarations.h"
#include "elevations-creation/LayerSeparator.h"
#include <future>
#include <memory>

#include "godot_cpp/core/print_string.hpp"
#include "godot_cpp/variant/variant.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

constexpr u8 CHUNK_SIZE = 64;

enum DiscreteLandTypeByHeight : u8;

class MapManager {

public:
    explicit MapManager(SeparatedMapResult sep) : elevations(std::move(sep.discrete)) {}

    MapManager(const MapManager& other)                = delete;
    MapManager& operator=(const MapManager& other)     = delete;

    MapManager(MapManager&& other) noexcept            = delete;
    MapManager& operator=(MapManager&& other) noexcept = delete;

    void positionUpdated(const godot::Variant& position);

private:

    std::unique_ptr<DiscreteLandTypeByHeight[]> elevations;
    std::future<void> futureResult;

    f32 tileHorizontalOffset   = 173.205078;
    f32 oddRowHorizontalOffset = 86.602539;
    f32 tileVerticalOffset     = 150.0;
};



#endif //MAPMANAGER_H
