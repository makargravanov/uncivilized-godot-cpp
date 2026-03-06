//
// Created by Copilot on 06.03.2026.
//

#ifndef TEMPERATUREPASS_H
#define TEMPERATUREPASS_H

#include <memory>

#include "game/map/TileData.h"
#include "util/declarations.h"

struct MapResult;

class TemperaturePass {
public:
    static void apply(std::unique_ptr<TileData[]>& tiles, const MapResult& mapResult);
    static f32 normalizeForOverlay(i8 temperatureCelsius);

private:
    static f64 getLatitudeRadians(u32 row, u32 height);
    static i8 quantizeTemperature(f32 temperatureCelsius);
};

#endif //TEMPERATUREPASS_H