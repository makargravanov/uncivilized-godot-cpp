#ifndef MOISTUREPASS_H
#define MOISTUREPASS_H

#include "ClimateState.h"
#include "game/map/TileData.h"
#include <memory>

class MoisturePass {
public:
    static void initialize(ClimateState& climateState);
    static void advanceOneTurn(ClimateState& climateState);
    static void publishToTiles(const ClimateState& climateState, const std::unique_ptr<TileData[]>& tiles);

    static f32 normalizeForOverlay(f32 humidityKgPerKg);
};

#endif //MOISTUREPASS_H
