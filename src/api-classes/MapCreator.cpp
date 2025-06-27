//
// Created by Alex on 20.06.2025.
//

#include "MapCreator.h"


#include "../game/map/elevations-creation/Aligner.h"
#include "../game/map/elevations-creation/LayerSeparator.h"

void MapCreator::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_heightmap_callback", "callback"), &MapCreator::set_heightmap_callback);
    godot::ClassDB::bind_method(godot::D_METHOD("set_finish_callback", "callback"), &MapCreator::set_finish_callback);
    godot::ClassDB::bind_method(godot::D_METHOD("create"), &MapCreator::create);
}

void MapCreator::create() {
    if (allCallbacksValid()) {
        createInternal();
    }
}
void MapCreator::createInternal() {
    //TODO: добавить callbacks для переходов между стадиями
    this->futureResult = std::async(std::launch::async, [this] {
        auto heights = PlatecWrapper::createHeights(MapArgs(), [this](f32 a, u8 b) { callbackHeightMap.call_deferred(a, b); });
        auto aligned = Aligner::applyBorders(Aligner::applyAlign(std::move(heights)));

        auto separated = LayerSeparator::initializeOceanAndThresholds(std::move(aligned));

        if (callbackFinish.is_valid()) {
            callbackFinish.call_deferred();
        }
    });
}

void MapCreator::set_heightmap_callback(const godot::Callable& callback) {
    this->callbackHeightMap = callback;
}
void MapCreator::set_finish_callback(const godot::Callable& callback) {
    this->callbackFinish = callback;
}
bool MapCreator::allCallbacksValid() const {
    if (!callbackHeightMap.is_valid()) {return false;}
    if (!callbackFinish.is_valid()) {return false;}
    return true;
}
MapCreator::~MapCreator() {
    if (futureResult.valid()) {
        futureResult.wait();
    }
}
