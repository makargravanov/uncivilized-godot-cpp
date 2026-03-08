#include "ClimateMetricsPass.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

void resetCurrentYearMetrics(ClimateState& climateState) {
    if (!climateState.annualPrecipitationAccumulator || !climateState.currentYearTemperatureSumKelvin ||
        !climateState.currentYearTemperatureMinKelvin || !climateState.currentYearTemperatureMaxKelvin) {
        climateState.currentYearTurnSamples = 0;
        return;
    }

    std::memset(climateState.annualPrecipitationAccumulator.get(), 0, climateState.tileCount * sizeof(f32));
    std::memset(climateState.currentYearTemperatureSumKelvin.get(), 0, climateState.tileCount * sizeof(f32));

    const f32 positiveInfinity = std::numeric_limits<f32>::infinity();
    const f32 negativeInfinity = -std::numeric_limits<f32>::infinity();
    for (u32 index = 0; index < climateState.tileCount; ++index) {
        climateState.currentYearTemperatureMinKelvin[index] = positiveInfinity;
        climateState.currentYearTemperatureMaxKelvin[index] = negativeInfinity;
    }

    climateState.currentYearTurnSamples = 0;
}

void finalizeCompletedYearMetrics(ClimateState& climateState) {
    if (!climateState.annualPrecipitationAccumulator || !climateState.currentYearTemperatureSumKelvin ||
        !climateState.currentYearTemperatureMinKelvin || !climateState.currentYearTemperatureMaxKelvin ||
        !climateState.completedAnnualPrecipitation || !climateState.completedAnnualMeanTemperatureKelvin ||
        !climateState.completedAnnualTemperatureMinKelvin || !climateState.completedAnnualTemperatureMaxKelvin ||
        climateState.currentYearTurnSamples == 0) {
        return;
    }

    const f32 inverseSampleCount = 1.0f / static_cast<f32>(climateState.currentYearTurnSamples);
    for (u32 index = 0; index < climateState.tileCount; ++index) {
        climateState.completedAnnualPrecipitation[index] = climateState.annualPrecipitationAccumulator[index];
        climateState.completedAnnualMeanTemperatureKelvin[index] =
            climateState.currentYearTemperatureSumKelvin[index] * inverseSampleCount;
        climateState.completedAnnualTemperatureMinKelvin[index] =
            climateState.currentYearTemperatureMinKelvin[index];
        climateState.completedAnnualTemperatureMaxKelvin[index] =
            climateState.currentYearTemperatureMaxKelvin[index];
    }

    ++climateState.completedClimateYears;
}

void accumulateCurrentTurnMetrics(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || !climateState.turnPrecipitation || !climateState.annualPrecipitationAccumulator ||
        !climateState.currentYearTemperatureSumKelvin || !climateState.currentYearTemperatureMinKelvin ||
        !climateState.currentYearTemperatureMaxKelvin) {
        return;
    }

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f32 temperatureKelvin = climateState.temperatureKelvin[index];
        climateState.annualPrecipitationAccumulator[index] += climateState.turnPrecipitation[index];
        climateState.currentYearTemperatureSumKelvin[index] += temperatureKelvin;
        climateState.currentYearTemperatureMinKelvin[index] = std::min(
            climateState.currentYearTemperatureMinKelvin[index],
            temperatureKelvin);
        climateState.currentYearTemperatureMaxKelvin[index] = std::max(
            climateState.currentYearTemperatureMaxKelvin[index],
            temperatureKelvin);
    }

    ++climateState.currentYearTurnSamples;
}

void beginNewYearIfNeeded(ClimateState& climateState) {
    if (climateState.currentTurnIndex != 0 || climateState.currentYearTurnSamples == 0) {
        return;
    }

    finalizeCompletedYearMetrics(climateState);
    resetCurrentYearMetrics(climateState);
}

} // namespace

void ClimateMetricsPass::initialize(ClimateState& climateState) {
    climateState.completedClimateYears = 0;
    resetCurrentYearMetrics(climateState);

    if (climateState.completedAnnualPrecipitation) {
        std::memset(climateState.completedAnnualPrecipitation.get(), 0, climateState.tileCount * sizeof(f32));
    }
    if (climateState.completedAnnualMeanTemperatureKelvin) {
        std::memset(climateState.completedAnnualMeanTemperatureKelvin.get(), 0, climateState.tileCount * sizeof(f32));
    }
    if (climateState.completedAnnualTemperatureMinKelvin) {
        std::memset(climateState.completedAnnualTemperatureMinKelvin.get(), 0, climateState.tileCount * sizeof(f32));
    }
    if (climateState.completedAnnualTemperatureMaxKelvin) {
        std::memset(climateState.completedAnnualTemperatureMaxKelvin.get(), 0, climateState.tileCount * sizeof(f32));
    }

    accumulateCurrentTurnMetrics(climateState);
}

void ClimateMetricsPass::advanceOneTurn(ClimateState& climateState) {
    beginNewYearIfNeeded(climateState);
    accumulateCurrentTurnMetrics(climateState);
}