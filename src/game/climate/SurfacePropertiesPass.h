#ifndef SURFACEPROPERTIESPASS_H
#define SURFACEPROPERTIESPASS_H

#include "ClimateState.h"

struct TileData;

class SurfacePropertiesPass {
public:
    static void initialize(ClimateState& climateState, const TileData* tiles);
    static void advanceOneTurn(ClimateState& climateState);
    static void refreshFromTiles(ClimateState& climateState, const TileData* tiles);
    static void publishToTiles(const ClimateState& climateState, TileData* tiles);
};

#endif //SURFACEPROPERTIESPASS_H