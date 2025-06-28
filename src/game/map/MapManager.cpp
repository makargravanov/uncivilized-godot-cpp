//
// Created by Alex on 27.06.2025.
//

#include "MapManager.h"
void MapManager::positionUpdated(const godot::Variant& position) {
    godot::Vector3 newPosition = position;
    godot::print_line("Camera position updated: ", newPosition);
}
