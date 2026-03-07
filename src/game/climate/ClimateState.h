//
// Created by Copilot on 06.03.2026.
//

#ifndef CLIMATESTATE_H
#define CLIMATESTATE_H

#include <cstring>
#include <memory>

#include "util/declarations.h"

struct ClimateState {
private:
    static std::unique_ptr<f32[]> copyBuffer(const std::unique_ptr<f32[]>& source, const u32 count) {
        if (!source || count == 0) {
            return nullptr;
        }

        auto destination = std::make_unique<f32[]>(count);
        std::memcpy(destination.get(), source.get(), count * sizeof(f32));
        return destination;
    }

public:
    u32 gridWidth = 0;
    u32 gridHeight = 0;
    u32 tileCount = 0;

    u64 absoluteTurnIndex = 0;
    u32 currentTurnIndex = 0;
    f32 currentYearFraction = 0.0f;

    // SoA buffers for the current climate state.
    std::unique_ptr<f32[]> temperatureKelvin;
    std::unique_ptr<f32[]> windEastMps;
    std::unique_ptr<f32[]> windNorthMps;
    std::unique_ptr<f32[]> humidityKgPerKg;
    std::unique_ptr<f32[]> turnPrecipitation;
    std::unique_ptr<f32[]> annualPrecipitationAccumulator;

    // Minimal static inputs needed by the temperature model.
    std::unique_ptr<f32[]> latitudeRadians;
    std::unique_ptr<f32[]> relativeAltitude;

    ClimateState() = default;

    ClimateState(u32 width, u32 height)
        : gridWidth(width),
          gridHeight(height),
          tileCount(width * height),
          temperatureKelvin(std::make_unique<f32[]>(tileCount)),
                    windEastMps(std::make_unique<f32[]>(tileCount)),
                    windNorthMps(std::make_unique<f32[]>(tileCount)),
          humidityKgPerKg(std::make_unique<f32[]>(tileCount)),
          turnPrecipitation(std::make_unique<f32[]>(tileCount)),
          annualPrecipitationAccumulator(std::make_unique<f32[]>(tileCount)),
          latitudeRadians(std::make_unique<f32[]>(tileCount)),
          relativeAltitude(std::make_unique<f32[]>(tileCount)) {}

    ClimateState(const ClimateState& other)
        : gridWidth(other.gridWidth),
          gridHeight(other.gridHeight),
          tileCount(other.tileCount),
          absoluteTurnIndex(other.absoluteTurnIndex),
          currentTurnIndex(other.currentTurnIndex),
          currentYearFraction(other.currentYearFraction),
          temperatureKelvin(copyBuffer(other.temperatureKelvin, other.tileCount)),
          windEastMps(copyBuffer(other.windEastMps, other.tileCount)),
          windNorthMps(copyBuffer(other.windNorthMps, other.tileCount)),
          humidityKgPerKg(copyBuffer(other.humidityKgPerKg, other.tileCount)),
          turnPrecipitation(copyBuffer(other.turnPrecipitation, other.tileCount)),
          annualPrecipitationAccumulator(copyBuffer(other.annualPrecipitationAccumulator, other.tileCount)),
          latitudeRadians(copyBuffer(other.latitudeRadians, other.tileCount)),
          relativeAltitude(copyBuffer(other.relativeAltitude, other.tileCount)) {}

    ClimateState& operator=(const ClimateState& other) {
        if (this == &other) {
            return *this;
        }

        gridWidth = other.gridWidth;
        gridHeight = other.gridHeight;
        tileCount = other.tileCount;
        absoluteTurnIndex = other.absoluteTurnIndex;
        currentTurnIndex = other.currentTurnIndex;
        currentYearFraction = other.currentYearFraction;

        temperatureKelvin = copyBuffer(other.temperatureKelvin, other.tileCount);
        windEastMps = copyBuffer(other.windEastMps, other.tileCount);
        windNorthMps = copyBuffer(other.windNorthMps, other.tileCount);
        humidityKgPerKg = copyBuffer(other.humidityKgPerKg, other.tileCount);
        turnPrecipitation = copyBuffer(other.turnPrecipitation, other.tileCount);
        annualPrecipitationAccumulator = copyBuffer(other.annualPrecipitationAccumulator, other.tileCount);
        latitudeRadians = copyBuffer(other.latitudeRadians, other.tileCount);
        relativeAltitude = copyBuffer(other.relativeAltitude, other.tileCount);

        return *this;
    }

    ClimateState(ClimateState&&) noexcept = default;
    ClimateState& operator=(ClimateState&&) noexcept = default;
};

#endif //CLIMATESTATE_H