//
// Created by Alex on 27.06.2025.
//

#ifndef SYSTEMNEXUS_H
#define SYSTEMNEXUS_H

#include <memory>

#include "api-classes/PlayScene.h"
#include "climate/ClimateState.h"
#include "climate/TemperaturePass.h"
#include "climate/WindPass.h"
#include "map/MapManager.h"
#include "map/BiomeClassifier.h"
#include "map/elevations-creation/LayerSeparator.h"

//FIXME: возможно, стоит улучшить, сделать нормальную регистрацию и авто-отписку через RAII-объект, но пока норм, хоть и костыльно
class SystemNexus {
public:
    static void configureMap(SeparatedMapResult mapResult) {
        auto tiles = BiomeClassifier::classify(
            mapResult.discrete,
            mapResult.mapResult.width,
            mapResult.mapResult.height);

        climateState = std::make_unique<ClimateState>(
            TemperaturePass::createInitialState(mapResult.mapResult));
        WindPass::initialize(*climateState);
        TemperaturePass::publishToTiles(*climateState, tiles);

        mapManager = new MapManager(
            std::move(tiles),
            mapResult.mapResult.width,
            mapResult.mapResult.height);
    }
    static MapManager* getMapManager() {
        return mapManager;
    }

    static ClimateState* getClimateState() {
        return climateState.get();
    }

    static void advanceClimateTurn() {
        if (!climateState || !mapManager) {
            return;
        }

        TemperaturePass::advanceOneTurn(*climateState);
        WindPass::advanceOneTurn(*climateState);
        mapManager->updateTemperatureSnapshot(*climateState);
    }

    static void advanceTemperatureTurn() {
        advanceClimateTurn();
    }

    static void regPlayScene(PlayScene* p) {
        play = p;
    }
    static PlayScene* playScene() {
        return play;
    }

    static void finalize() {
        delete mapManager;
        mapManager = nullptr;
        climateState.reset();
    }

private:
    static MapManager* mapManager;
    static std::unique_ptr<ClimateState> climateState;
    static PlayScene* play;
};

#endif //SYSTEMNEXUS_H
