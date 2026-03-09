//
// Created by Copilot on 06.03.2026.
//

#ifndef TEMPERATUREPASS_H
#define TEMPERATUREPASS_H

#include <memory>

#include "ClimateState.h"
#include "game/map/TileData.h"
#include "util/declarations.h"

struct MapResult;

class TemperaturePass {
public:
    static ClimateState createInitialState(const MapResult& mapResult);
    static void initializeCurrentTurn(ClimateState& climateState);
    static void advanceOneTurn(ClimateState& climateState);
    static void publishToTiles(const ClimateState& climateState, std::unique_ptr<TileData[]>& tiles);

    static f32 normalizeForOverlay(i8 temperatureCelsius);
    static f32 normalizeKelvinForOverlay(f32 temperatureKelvin);

private:
    static f64 getLatitudeRadians(u32 row, u32 height);
    static i8 quantizeTemperature(f32 temperatureCelsius);
    static void precomputeSeaLevelTemperatureLookup(ClimateState& climateState);
    static void blendTowardTurnTarget(ClimateState& climateState, u32 turnIndex, f32 blendFactor);
};

#endif //TEMPERATUREPASS_H