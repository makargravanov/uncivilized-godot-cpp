//
// Created by Copilot on 06.03.2026.
//

#ifndef CLIMATESTATE_H
#define CLIMATESTATE_H

#include <array>
#include <cstring>
#include <memory>

#include "util/declarations.h"

constexpr u32 CLIMATE_QUARTER_COUNT = 4;

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

    template <typename T, std::size_t N>
    static std::array<std::unique_ptr<T[]>, N> copyBufferArray(
        const std::array<std::unique_ptr<T[]>, N>& source,
        const u32 count) {
        std::array<std::unique_ptr<T[]>, N> destination {};
        for (std::size_t index = 0; index < N; ++index) {
            destination[index] = copyBuffer(source[index], count);
        }
        return destination;
    }

    template <typename T, std::size_t N>
    static std::array<std::unique_ptr<T[]>, N> allocateBufferArray(const u32 count) {
        std::array<std::unique_ptr<T[]>, N> destination {};
        for (std::size_t index = 0; index < N; ++index) {
            destination[index] = allocateBuffer<T>(count);
        }
        return destination;
    }

public:
    u32 gridWidth = 0;
    u32 gridHeight = 0;
    u32 tileCount = 0;

    u64 absoluteTurnIndex = 0;
    u32 currentTurnIndex = 0;
    f32 currentYearFraction = 0.0f;
    u32 currentYearTurnSamples = 0;
    std::array<u32, CLIMATE_QUARTER_COUNT> currentQuarterTurnSamples {};
    u32 completedClimateYears = 0;

    // SoA buffers for the current climate state.
    std::unique_ptr<f32[]> temperatureKelvin;
    std::unique_ptr<f32[]> windEastMps;
    std::unique_ptr<f32[]> windNorthMps;
    std::unique_ptr<f32[]> humidityKgPerKg;
    std::unique_ptr<f32[]> turnPrecipitation;
    std::unique_ptr<f32[]> annualPrecipitationAccumulator;
    std::unique_ptr<f32[]> humidityScratchKgPerKg;
    std::unique_ptr<f32[]> currentYearTemperatureSumKelvin;
    std::unique_ptr<f32[]> currentYearTemperatureMinKelvin;
    std::unique_ptr<f32[]> currentYearTemperatureMaxKelvin;
    std::array<std::unique_ptr<f32[]>, CLIMATE_QUARTER_COUNT> currentQuarterTemperatureSumKelvin;
    std::array<std::unique_ptr<f32[]>, CLIMATE_QUARTER_COUNT> currentQuarterPrecipitationAccumulator;
    std::unique_ptr<f32[]> completedAnnualPrecipitation;
    std::unique_ptr<f32[]> completedAnnualMeanTemperatureKelvin;
    std::unique_ptr<f32[]> completedAnnualTemperatureMinKelvin;
    std::unique_ptr<f32[]> completedAnnualTemperatureMaxKelvin;
    std::unique_ptr<f32[]> completedColdestQuarterMeanTemperatureKelvin;
    std::unique_ptr<f32[]> completedWarmestQuarterMeanTemperatureKelvin;
    std::unique_ptr<f32[]> completedDriestQuarterPrecipitation;
    std::unique_ptr<f32[]> completedWettestQuarterPrecipitation;
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
          currentYearTemperatureSumKelvin(std::make_unique<f32[]>(tileCount)),
          currentYearTemperatureMinKelvin(std::make_unique<f32[]>(tileCount)),
          currentYearTemperatureMaxKelvin(std::make_unique<f32[]>(tileCount)),
          currentQuarterTemperatureSumKelvin(allocateBufferArray<f32, CLIMATE_QUARTER_COUNT>(tileCount)),
          currentQuarterPrecipitationAccumulator(allocateBufferArray<f32, CLIMATE_QUARTER_COUNT>(tileCount)),
          completedAnnualPrecipitation(std::make_unique<f32[]>(tileCount)),
          completedAnnualMeanTemperatureKelvin(std::make_unique<f32[]>(tileCount)),
          completedAnnualTemperatureMinKelvin(std::make_unique<f32[]>(tileCount)),
          completedAnnualTemperatureMaxKelvin(std::make_unique<f32[]>(tileCount)),
          completedColdestQuarterMeanTemperatureKelvin(std::make_unique<f32[]>(tileCount)),
          completedWarmestQuarterMeanTemperatureKelvin(std::make_unique<f32[]>(tileCount)),
          completedDriestQuarterPrecipitation(std::make_unique<f32[]>(tileCount)),
          completedWettestQuarterPrecipitation(std::make_unique<f32[]>(tileCount)),
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
          currentYearTurnSamples(other.currentYearTurnSamples),
          currentQuarterTurnSamples(other.currentQuarterTurnSamples),
          completedClimateYears(other.completedClimateYears),
          temperatureKelvin(copyBuffer(other.temperatureKelvin, other.tileCount)),
          windEastMps(copyBuffer(other.windEastMps, other.tileCount)),
          windNorthMps(copyBuffer(other.windNorthMps, other.tileCount)),
          humidityKgPerKg(copyBuffer(other.humidityKgPerKg, other.tileCount)),
          turnPrecipitation(copyBuffer(other.turnPrecipitation, other.tileCount)),
          annualPrecipitationAccumulator(copyBuffer(other.annualPrecipitationAccumulator, other.tileCount)),
          humidityScratchKgPerKg(allocateBuffer<f32>(other.tileCount)),
          currentYearTemperatureSumKelvin(copyBuffer(other.currentYearTemperatureSumKelvin, other.tileCount)),
          currentYearTemperatureMinKelvin(copyBuffer(other.currentYearTemperatureMinKelvin, other.tileCount)),
          currentYearTemperatureMaxKelvin(copyBuffer(other.currentYearTemperatureMaxKelvin, other.tileCount)),
          currentQuarterTemperatureSumKelvin(copyBufferArray<f32, CLIMATE_QUARTER_COUNT>(
              other.currentQuarterTemperatureSumKelvin,
              other.tileCount)),
          currentQuarterPrecipitationAccumulator(copyBufferArray<f32, CLIMATE_QUARTER_COUNT>(
              other.currentQuarterPrecipitationAccumulator,
              other.tileCount)),
          completedAnnualPrecipitation(copyBuffer(other.completedAnnualPrecipitation, other.tileCount)),
          completedAnnualMeanTemperatureKelvin(copyBuffer(other.completedAnnualMeanTemperatureKelvin, other.tileCount)),
          completedAnnualTemperatureMinKelvin(copyBuffer(other.completedAnnualTemperatureMinKelvin, other.tileCount)),
          completedAnnualTemperatureMaxKelvin(copyBuffer(other.completedAnnualTemperatureMaxKelvin, other.tileCount)),
          completedColdestQuarterMeanTemperatureKelvin(copyBuffer(
              other.completedColdestQuarterMeanTemperatureKelvin,
              other.tileCount)),
          completedWarmestQuarterMeanTemperatureKelvin(copyBuffer(
              other.completedWarmestQuarterMeanTemperatureKelvin,
              other.tileCount)),
          completedDriestQuarterPrecipitation(copyBuffer(other.completedDriestQuarterPrecipitation, other.tileCount)),
          completedWettestQuarterPrecipitation(copyBuffer(other.completedWettestQuarterPrecipitation, other.tileCount)),
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
        currentYearTurnSamples = other.currentYearTurnSamples;
        currentQuarterTurnSamples = other.currentQuarterTurnSamples;
        completedClimateYears = other.completedClimateYears;

        temperatureKelvin = copyBuffer(other.temperatureKelvin, other.tileCount);
        windEastMps = copyBuffer(other.windEastMps, other.tileCount);
        windNorthMps = copyBuffer(other.windNorthMps, other.tileCount);
        humidityKgPerKg = copyBuffer(other.humidityKgPerKg, other.tileCount);
        turnPrecipitation = copyBuffer(other.turnPrecipitation, other.tileCount);
        annualPrecipitationAccumulator = copyBuffer(other.annualPrecipitationAccumulator, other.tileCount);
        humidityScratchKgPerKg = allocateBuffer<f32>(other.tileCount);
        currentYearTemperatureSumKelvin = copyBuffer(other.currentYearTemperatureSumKelvin, other.tileCount);
        currentYearTemperatureMinKelvin = copyBuffer(other.currentYearTemperatureMinKelvin, other.tileCount);
        currentYearTemperatureMaxKelvin = copyBuffer(other.currentYearTemperatureMaxKelvin, other.tileCount);
        currentQuarterTemperatureSumKelvin = copyBufferArray<f32, CLIMATE_QUARTER_COUNT>(
            other.currentQuarterTemperatureSumKelvin,
            other.tileCount);
        currentQuarterPrecipitationAccumulator = copyBufferArray<f32, CLIMATE_QUARTER_COUNT>(
            other.currentQuarterPrecipitationAccumulator,
            other.tileCount);
        completedAnnualPrecipitation = copyBuffer(other.completedAnnualPrecipitation, other.tileCount);
        completedAnnualMeanTemperatureKelvin = copyBuffer(other.completedAnnualMeanTemperatureKelvin, other.tileCount);
        completedAnnualTemperatureMinKelvin = copyBuffer(other.completedAnnualTemperatureMinKelvin, other.tileCount);
        completedAnnualTemperatureMaxKelvin = copyBuffer(other.completedAnnualTemperatureMaxKelvin, other.tileCount);
        completedColdestQuarterMeanTemperatureKelvin = copyBuffer(
            other.completedColdestQuarterMeanTemperatureKelvin,
            other.tileCount);
        completedWarmestQuarterMeanTemperatureKelvin = copyBuffer(
            other.completedWarmestQuarterMeanTemperatureKelvin,
            other.tileCount);
        completedDriestQuarterPrecipitation = copyBuffer(other.completedDriestQuarterPrecipitation, other.tileCount);
        completedWettestQuarterPrecipitation = copyBuffer(other.completedWettestQuarterPrecipitation, other.tileCount);
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