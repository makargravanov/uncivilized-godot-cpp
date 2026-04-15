//
// Created by Alex on 27.06.2025.
//

#ifndef SYSTEMNEXUS_H
#define SYSTEMNEXUS_H

#include <chrono>
#include <future>
#include <memory>

#include "api-classes/PlayScene.h"
#include "climate/ClimateConfig.h"
#include "climate/ClimateState.h"
#include "climate/ClimateMetricsPass.h"
#include "climate/ClimateRegulatorPass.h"
#include "climate/MoisturePass.h"
#include "climate/SurfacePropertiesPass.h"
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
        applyClimateRegulatorSettings(*climateState);
        SurfacePropertiesPass::initialize(*climateState, tiles.get());
        TemperaturePass::initializeCurrentTurn(*climateState);
        WindPass::initialize(*climateState);
        MoisturePass::initialize(*climateState);
        SurfacePropertiesPass::publishToTiles(*climateState, tiles.get());
        ClimateMetricsPass::initialize(*climateState);
        ClimateRegulatorPass::initialize(*climateState);
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

    static void setClimateRegulatorCorrectionEnabled(const bool enabled) {
        climateRegulatorRuntimeConfig.correctionEnabled = enabled;
        if (climateState) {
            climateState->regulatorConfig.correctionEnabled = enabled;
        }
    }

    static bool isClimateRegulatorCorrectionEnabled() {
        return climateRegulatorRuntimeConfig.correctionEnabled;
    }

    static void setClimateRegulatorTargetTemperatureC(const f32 temperatureC) {
        climateRegulatorRuntimeConfig.targetGlobalMeanTemperatureC = temperatureC;
        if (climateState) {
            climateState->regulatorConfig.targetGlobalMeanTemperatureC = temperatureC;
        }
    }

    static f32 climateRegulatorTargetTemperatureCValue() {
        return climateRegulatorRuntimeConfig.targetGlobalMeanTemperatureC;
    }

    static void setClimateRegulatorInsolationEnabled(const bool enabled) {
        climateRegulatorRuntimeConfig.insolation.enabled = enabled;
        if (climateState) {
            climateState->regulatorConfig.insolation.enabled = enabled;
        }
    }

    static bool isClimateRegulatorInsolationEnabled() {
        return climateRegulatorRuntimeConfig.insolation.enabled;
    }

    static void setClimateRegulatorInsolationStrength(const f32 strength) {
        climateRegulatorRuntimeConfig.insolation.strength = strength;
        if (climateState) {
            climateState->regulatorConfig.insolation.strength = strength;
        }
    }

    static f32 climateRegulatorInsolationStrength() {
        return climateRegulatorRuntimeConfig.insolation.strength;
    }

    static void setClimateRegulatorInsolationMaxMagnitude(const f32 maxMagnitude) {
        climateRegulatorRuntimeConfig.insolation.maxMagnitude = maxMagnitude;
        if (climateState) {
            climateState->regulatorConfig.insolation.maxMagnitude = maxMagnitude;
        }
    }

    static f32 climateRegulatorInsolationMaxMagnitude() {
        return climateRegulatorRuntimeConfig.insolation.maxMagnitude;
    }

    static void setClimateRegulatorCryosphereAlbedoEnabled(const bool enabled) {
        climateRegulatorRuntimeConfig.cryosphereAlbedo.enabled = enabled;
        if (climateState) {
            climateState->regulatorConfig.cryosphereAlbedo.enabled = enabled;
        }
    }

    static bool isClimateRegulatorCryosphereAlbedoEnabled() {
        return climateRegulatorRuntimeConfig.cryosphereAlbedo.enabled;
    }

    static void setClimateRegulatorCryosphereAlbedoStrength(const f32 strength) {
        climateRegulatorRuntimeConfig.cryosphereAlbedo.strength = strength;
        if (climateState) {
            climateState->regulatorConfig.cryosphereAlbedo.strength = strength;
        }
    }

    static f32 climateRegulatorCryosphereAlbedoStrength() {
        return climateRegulatorRuntimeConfig.cryosphereAlbedo.strength;
    }

    static void setClimateRegulatorCryosphereAlbedoMaxMagnitude(const f32 maxMagnitude) {
        climateRegulatorRuntimeConfig.cryosphereAlbedo.maxMagnitude = maxMagnitude;
        if (climateState) {
            climateState->regulatorConfig.cryosphereAlbedo.maxMagnitude = maxMagnitude;
        }
    }

    static f32 climateRegulatorCryosphereAlbedoMaxMagnitude() {
        return climateRegulatorRuntimeConfig.cryosphereAlbedo.maxMagnitude;
    }

    static void setClimateRegulatorBaseAlbedoEnabled(const bool enabled) {
        climateRegulatorRuntimeConfig.baseAlbedo.enabled = enabled;
        if (climateState) {
            climateState->regulatorConfig.baseAlbedo.enabled = enabled;
        }
    }

    static bool isClimateRegulatorBaseAlbedoEnabled() {
        return climateRegulatorRuntimeConfig.baseAlbedo.enabled;
    }

    static void setClimateRegulatorBaseAlbedoStrength(const f32 strength) {
        climateRegulatorRuntimeConfig.baseAlbedo.strength = strength;
        if (climateState) {
            climateState->regulatorConfig.baseAlbedo.strength = strength;
        }
    }

    static f32 climateRegulatorBaseAlbedoStrength() {
        return climateRegulatorRuntimeConfig.baseAlbedo.strength;
    }

    static void setClimateRegulatorBaseAlbedoMaxMagnitude(const f32 maxMagnitude) {
        climateRegulatorRuntimeConfig.baseAlbedo.maxMagnitude = maxMagnitude;
        if (climateState) {
            climateState->regulatorConfig.baseAlbedo.maxMagnitude = maxMagnitude;
        }
    }

    static f32 climateRegulatorBaseAlbedoMaxMagnitude() {
        return climateRegulatorRuntimeConfig.baseAlbedo.maxMagnitude;
    }

    static void advanceClimateTurn() {
        if (!climateState || !mapManager) {
            return;
        }

        advanceClimateStateOneTurn(*climateState);
        const bool biomeSnapshotUpdated = updateBiomeSnapshotIfNeeded();
        if (biomeSnapshotUpdated) {
            SurfacePropertiesPass::refreshFromTiles(*climateState, mapManager->getTiles());
        }
        SurfacePropertiesPass::publishToTiles(*climateState, mapManager->getTiles());
        mapManager->updateTemperatureSnapshot(*climateState);
        mapManager->updateSurfaceSnapshot();
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

        applyClimateRegulatorSettings(*climateState);

        if (mapManager) {
            const bool biomeSnapshotUpdated = updateBiomeSnapshotIfNeeded();
            if (biomeSnapshotUpdated) {
                SurfacePropertiesPass::refreshFromTiles(*climateState, mapManager->getTiles());
            }
            SurfacePropertiesPass::publishToTiles(*climateState, mapManager->getTiles());
            mapManager->updateTemperatureSnapshot(*climateState);
            mapManager->updateSurfaceSnapshot();
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
        SurfacePropertiesPass::advanceOneTurn(state);
        ClimateMetricsPass::advanceOneTurn(state);
        ClimateRegulatorPass::advanceOneTurn(state);
    }

    static bool updateBiomeSnapshotIfNeeded() {
        if (!mapManager || !climateState || climateState->completedClimateYears <= appliedClimateBiomeYears) {
            return false;
        }

        const bool biomeSnapshotUpdated = mapManager->updateBiomeSnapshot(*climateState);
        appliedClimateBiomeYears = climateState->completedClimateYears;
        return biomeSnapshotUpdated;
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
    static ClimateRegulatorRuntimeConfig climateRegulatorRuntimeConfig;

    static void copyActuatorSettings(
        ClimateRegulatorActuatorRuntime& destination,
        const ClimateRegulatorActuatorRuntime& source) {
        destination.enabled = source.enabled;
        destination.strength = source.strength;
        destination.maxMagnitude = source.maxMagnitude;
    }

    static void applyClimateRegulatorSettings(ClimateState& state) {
        state.regulatorConfig.correctionEnabled = climateRegulatorRuntimeConfig.correctionEnabled;
        state.regulatorConfig.targetGlobalMeanTemperatureC =
            climateRegulatorRuntimeConfig.targetGlobalMeanTemperatureC;
        copyActuatorSettings(state.regulatorConfig.insolation, climateRegulatorRuntimeConfig.insolation);
        copyActuatorSettings(
            state.regulatorConfig.cryosphereAlbedo,
            climateRegulatorRuntimeConfig.cryosphereAlbedo);
        copyActuatorSettings(state.regulatorConfig.baseAlbedo, climateRegulatorRuntimeConfig.baseAlbedo);
    }
};

#endif //SYSTEMNEXUS_H
