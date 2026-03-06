//
// Created by Copilot on 06.03.2026.
//

#ifndef CLIMATESTATE_H
#define CLIMATESTATE_H

#include <memory>

#include "util/declarations.h"

struct ClimateState {
    u32 gridWidth = 0;
    u32 gridHeight = 0;
    u32 tileCount = 0;

    u32 currentTurnIndex = 0;
    f32 currentYearFraction = 0.0f;

    // SoA buffers for the current climate state.
    std::unique_ptr<f32[]> temperatureKelvin;

    // Minimal static inputs needed by the temperature model.
    std::unique_ptr<f32[]> latitudeRadians;
    std::unique_ptr<f32[]> relativeAltitude;

    ClimateState() = default;

    ClimateState(u32 width, u32 height)
        : gridWidth(width),
          gridHeight(height),
          tileCount(width * height),
          temperatureKelvin(std::make_unique<f32[]>(tileCount)),
          latitudeRadians(std::make_unique<f32[]>(tileCount)),
          relativeAltitude(std::make_unique<f32[]>(tileCount)) {}

    ClimateState(const ClimateState&) = delete;
    ClimateState& operator=(const ClimateState&) = delete;
    ClimateState(ClimateState&&) noexcept = default;
    ClimateState& operator=(ClimateState&&) noexcept = default;
};

#endif //CLIMATESTATE_H