//
// Created by Alex on 27.06.2025.
//

#include "SystemNexus.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

ClimateRegulatorRuntimeConfig buildDefaultClimateRegulatorRuntimeConfig() {
    ClimateRegulatorRuntimeConfig runtimeConfig;
    runtimeConfig.correctionEnabled = false;
    runtimeConfig.targetGlobalMeanTemperatureC = CONFIG.regulator.targetGlobalMeanTemperatureC;
    runtimeConfig.insolation.enabled = CONFIG.regulator.defaultInsolationEnabled;
    runtimeConfig.insolation.strength = CONFIG.regulator.defaultInsolationStrength;
    runtimeConfig.insolation.maxMagnitude = CONFIG.regulator.defaultInsolationMaxMagnitude;
    runtimeConfig.cryosphereAlbedo.enabled = CONFIG.regulator.defaultCryosphereAlbedoEnabled;
    runtimeConfig.cryosphereAlbedo.strength = CONFIG.regulator.defaultCryosphereAlbedoStrength;
    runtimeConfig.cryosphereAlbedo.maxMagnitude = CONFIG.regulator.defaultCryosphereAlbedoMaxMagnitude;
    runtimeConfig.baseAlbedo.enabled = CONFIG.regulator.defaultBaseAlbedoEnabled;
    runtimeConfig.baseAlbedo.strength = CONFIG.regulator.defaultBaseAlbedoStrength;
    runtimeConfig.baseAlbedo.maxMagnitude = CONFIG.regulator.defaultBaseAlbedoMaxMagnitude;
    return runtimeConfig;
}

} // namespace

MapManager* SystemNexus::mapManager = nullptr;
std::unique_ptr<ClimateState> SystemNexus::climateState = nullptr;
std::future<ClimateState> SystemNexus::pendingClimateTurn;
bool SystemNexus::climateTurnInProgress = false;
u32 SystemNexus::appliedClimateBiomeYears = 0;
PlayScene* SystemNexus::play = nullptr;
ClimateRegulatorRuntimeConfig SystemNexus::climateRegulatorRuntimeConfig =
    buildDefaultClimateRegulatorRuntimeConfig();
