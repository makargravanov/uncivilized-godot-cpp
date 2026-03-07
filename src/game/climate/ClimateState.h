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
    template <typename T>
    static std::unique_ptr<T[]> copyBuffer(const std::unique_ptr<T[]>& source, const u32 count) {
        if (!source || count == 0) {
            return nullptr;
        }

        auto destination = std::make_unique<T[]>(count);
        std::memcpy(destination.get(), source.get(), count * sizeof(T));
        return destination;
    }

    template <typename T>
    static std::unique_ptr<T[]> allocateBuffer(const u32 count) {
        if (count == 0) {
            return nullptr;
        }

        return std::make_unique<T[]>(count);
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
    std::unique_ptr<f32[]> humidityScratchKgPerKg;
    std::unique_ptr<u32[]> moistureUpwindEastIndex;
    std::unique_ptr<u32[]> moistureUpwindNorthIndex;
    std::unique_ptr<f32[]> moistureEastWeight;
    std::unique_ptr<f32[]> moistureNorthWeight;
    std::unique_ptr<f32[]> moistureMixingFactor;
    std::unique_ptr<f32[]> moistureOrographicCoolingK;

    // Minimal static inputs needed by the temperature model.
    std::unique_ptr<f32[]> latitudeRadians;
    std::unique_ptr<f32[]> relativeAltitude;
    u32 seaLevelTemperatureTurnCount = 0;
    std::unique_ptr<f32[]> seaLevelTemperatureByTurnRow;

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
          humidityScratchKgPerKg(std::make_unique<f32[]>(tileCount)),
          moistureUpwindEastIndex(std::make_unique<u32[]>(tileCount)),
          moistureUpwindNorthIndex(std::make_unique<u32[]>(tileCount)),
          moistureEastWeight(std::make_unique<f32[]>(tileCount)),
          moistureNorthWeight(std::make_unique<f32[]>(tileCount)),
          moistureMixingFactor(std::make_unique<f32[]>(tileCount)),
          moistureOrographicCoolingK(std::make_unique<f32[]>(tileCount)),
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
          humidityScratchKgPerKg(allocateBuffer<f32>(other.tileCount)),
          moistureUpwindEastIndex(allocateBuffer<u32>(other.tileCount)),
          moistureUpwindNorthIndex(allocateBuffer<u32>(other.tileCount)),
          moistureEastWeight(allocateBuffer<f32>(other.tileCount)),
          moistureNorthWeight(allocateBuffer<f32>(other.tileCount)),
          moistureMixingFactor(allocateBuffer<f32>(other.tileCount)),
          moistureOrographicCoolingK(allocateBuffer<f32>(other.tileCount)),
          latitudeRadians(copyBuffer(other.latitudeRadians, other.tileCount)),
          relativeAltitude(copyBuffer(other.relativeAltitude, other.tileCount)),
          seaLevelTemperatureTurnCount(other.seaLevelTemperatureTurnCount),
          seaLevelTemperatureByTurnRow(copyBuffer(other.seaLevelTemperatureByTurnRow,
              other.seaLevelTemperatureTurnCount * other.gridHeight)) {}

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
        humidityScratchKgPerKg = allocateBuffer<f32>(other.tileCount);
        moistureUpwindEastIndex = allocateBuffer<u32>(other.tileCount);
        moistureUpwindNorthIndex = allocateBuffer<u32>(other.tileCount);
        moistureEastWeight = allocateBuffer<f32>(other.tileCount);
        moistureNorthWeight = allocateBuffer<f32>(other.tileCount);
        moistureMixingFactor = allocateBuffer<f32>(other.tileCount);
        moistureOrographicCoolingK = allocateBuffer<f32>(other.tileCount);
        latitudeRadians = copyBuffer(other.latitudeRadians, other.tileCount);
        relativeAltitude = copyBuffer(other.relativeAltitude, other.tileCount);
        seaLevelTemperatureTurnCount = other.seaLevelTemperatureTurnCount;
        seaLevelTemperatureByTurnRow = copyBuffer(
            other.seaLevelTemperatureByTurnRow,
            other.seaLevelTemperatureTurnCount * other.gridHeight);

        return *this;
    }

    ClimateState(ClimateState&&) noexcept = default;
    ClimateState& operator=(ClimateState&&) noexcept = default;
};

#endif //CLIMATESTATE_H