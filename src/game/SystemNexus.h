//
// Created by Alex on 27.06.2025.
//

#ifndef SYSTEMNEXUS_H
#define SYSTEMNEXUS_H

#include <chrono>
#include <future>
#include <memory>

#include "api-classes/PlayScene.h"
#include "climate/ClimateState.h"
#include "climate/ClimateMetricsPass.h"
#include "climate/MoisturePass.h"
#include "climate/TemperaturePass.h"
#include "climate/WindPass.h"
#include "map/MapManager.h"
#include "map/BiomeClassifier.h"
#include "map/elevations-creation/LayerSeparator.h"

//FIXME: возможно, стоит улучшить, сделать нормальную регистрацию и авто-отписку через RAII-объект, но пока норм, хоть и костыльно
class SystemNexus {
public:
    static void configureMap(SeparatedMapResult mapResult) {
        discardPendingClimateTurn();
        delete mapManager;
        mapManager = nullptr;
        appliedClimateBiomeYears = 0;

        auto tiles = BiomeClassifier::classify(
            mapResult.discrete,
            mapResult.mapResult.width,
            mapResult.mapResult.height);

        climateState = std::make_unique<ClimateState>(
            TemperaturePass::createInitialState(mapResult.mapResult));
        WindPass::initialize(*climateState);
        MoisturePass::initialize(*climateState);
        ClimateMetricsPass::initialize(*climateState);
        TemperaturePass::publishToTiles(*climateState, tiles);
        MoisturePass::publishToTiles(*climateState, tiles);

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

        advanceClimateStateOneTurn(*climateState);
        mapManager->updateTemperatureSnapshot(*climateState);
        updateBiomeSnapshotIfNeeded();
    }

    static bool requestClimateTurnAsync() {
        if (!climateState || !mapManager || climateTurnInProgress) {
            return false;
        }

        ClimateState nextClimateState(*climateState);
        climateTurnInProgress = true;
        pendingClimateTurn = std::async(
            std::launch::async,
            [state = std::move(nextClimateState)]() mutable -> ClimateState {
                advanceClimateStateOneTurn(state);
                return state;
            });
        return true;
    }

    static bool applyCompletedClimateTurn() {
        if (!climateTurnInProgress || !pendingClimateTurn.valid()) {
            return false;
        }

        if (pendingClimateTurn.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }

        ClimateState completedClimateState = pendingClimateTurn.get();
        climateTurnInProgress = false;

        if (climateState) {
            *climateState = std::move(completedClimateState);
        } else {
            climateState = std::make_unique<ClimateState>(std::move(completedClimateState));
        }

        if (mapManager) {
            mapManager->updateTemperatureSnapshot(*climateState);
            updateBiomeSnapshotIfNeeded();
        }

        return true;
    }

    static bool isClimateTurnInProgress() {
        return climateTurnInProgress;
    }

    static u64 currentClimateTurn() {
        return climateState ? climateState->absoluteTurnIndex : 0;
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
        discardPendingClimateTurn();
        delete mapManager;
        mapManager = nullptr;
        climateState.reset();
    }

private:
    static void advanceClimateStateOneTurn(ClimateState& state) {
        TemperaturePass::advanceOneTurn(state);
        WindPass::advanceOneTurn(state);
        MoisturePass::advanceOneTurn(state);
        ClimateMetricsPass::advanceOneTurn(state);
    }

    static void updateBiomeSnapshotIfNeeded() {
        if (!mapManager || !climateState || climateState->completedClimateYears <= appliedClimateBiomeYears) {
            return;
        }

        mapManager->updateBiomeSnapshot(*climateState);
        appliedClimateBiomeYears = climateState->completedClimateYears;
    }

    static void discardPendingClimateTurn() {
        if (pendingClimateTurn.valid()) {
            pendingClimateTurn.wait();
            pendingClimateTurn.get();
        }
        climateTurnInProgress = false;
    }

    static MapManager* mapManager;
    static std::unique_ptr<ClimateState> climateState;
    static std::future<ClimateState> pendingClimateTurn;
    static bool climateTurnInProgress;
    static u32 appliedClimateBiomeYears;
    static PlayScene* play;
};

#endif //SYSTEMNEXUS_H
