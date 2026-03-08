#include "ClimateMetricsPass.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

void clearBuffer(const std::unique_ptr<f32[]>& buffer, const u32 tileCount) {
    if (!buffer || tileCount == 0) {
        return;
    }

    std::memset(buffer.get(), 0, tileCount * sizeof(f32));
}

u32 getQuarterIndex(const ClimateState& climateState) {
    const u32 yearTurnCount = std::max(climateState.seaLevelTemperatureTurnCount, 1u);
    const u32 quarterIndex =
        (climateState.currentTurnIndex * CLIMATE_QUARTER_COUNT) / yearTurnCount;
    return std::min(quarterIndex, CLIMATE_QUARTER_COUNT - 1);
}

void resetCurrentYearMetrics(ClimateState& climateState) {
    climateState.currentYearTurnSamples = 0;
    climateState.currentQuarterTurnSamples.fill(0);

    clearBuffer(climateState.annualPrecipitationAccumulator, climateState.tileCount);
    clearBuffer(climateState.currentYearTemperatureSumKelvin, climateState.tileCount);
    for (u32 quarterIndex = 0; quarterIndex < CLIMATE_QUARTER_COUNT; ++quarterIndex) {
        clearBuffer(climateState.currentQuarterTemperatureSumKelvin[quarterIndex], climateState.tileCount);
        clearBuffer(climateState.currentQuarterPrecipitationAccumulator[quarterIndex], climateState.tileCount);
    }

    if (!climateState.currentYearTemperatureMinKelvin || !climateState.currentYearTemperatureMaxKelvin) {
        return;
    }

    const f32 positiveInfinity = std::numeric_limits<f32>::infinity();
    const f32 negativeInfinity = -std::numeric_limits<f32>::infinity();
    for (u32 index = 0; index < climateState.tileCount; ++index) {
        climateState.currentYearTemperatureMinKelvin[index] = positiveInfinity;
        climateState.currentYearTemperatureMaxKelvin[index] = negativeInfinity;
    }
}

void finalizeCompletedYearMetrics(ClimateState& climateState) {
    if (!climateState.annualPrecipitationAccumulator || !climateState.currentYearTemperatureSumKelvin ||
        !climateState.currentYearTemperatureMinKelvin || !climateState.currentYearTemperatureMaxKelvin ||
        !climateState.completedAnnualPrecipitation || !climateState.completedAnnualMeanTemperatureKelvin ||
        !climateState.completedAnnualTemperatureMinKelvin || !climateState.completedAnnualTemperatureMaxKelvin ||
        !climateState.completedColdestQuarterMeanTemperatureKelvin ||
        !climateState.completedWarmestQuarterMeanTemperatureKelvin ||
        !climateState.completedDriestQuarterPrecipitation ||
        !climateState.completedWettestQuarterPrecipitation ||
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

        f32 coldestQuarterTemperature = climateState.completedAnnualMeanTemperatureKelvin[index];
        f32 warmestQuarterTemperature = climateState.completedAnnualMeanTemperatureKelvin[index];
        bool hasQuarterTemperature = false;

        f32 driestQuarterPrecipitation = climateState.completedAnnualPrecipitation[index];
        f32 wettestQuarterPrecipitation = climateState.completedAnnualPrecipitation[index];
        bool hasQuarterPrecipitation = false;

        for (u32 quarterIndex = 0; quarterIndex < CLIMATE_QUARTER_COUNT; ++quarterIndex) {
            const u32 sampleCount = climateState.currentQuarterTurnSamples[quarterIndex];
            if (sampleCount == 0) {
                continue;
            }

            if (const f32* quarterTemperatureSums =
                    climateState.currentQuarterTemperatureSumKelvin[quarterIndex].get();
                quarterTemperatureSums) {
                const f32 quarterMeanTemperature =
                    quarterTemperatureSums[index] / static_cast<f32>(sampleCount);
                if (!hasQuarterTemperature) {
                    coldestQuarterTemperature = quarterMeanTemperature;
                    warmestQuarterTemperature = quarterMeanTemperature;
                    hasQuarterTemperature = true;
                } else {
                    coldestQuarterTemperature = std::min(coldestQuarterTemperature, quarterMeanTemperature);
                    warmestQuarterTemperature = std::max(warmestQuarterTemperature, quarterMeanTemperature);
                }
            }

            if (const f32* quarterPrecipitationSums =
                    climateState.currentQuarterPrecipitationAccumulator[quarterIndex].get();
                quarterPrecipitationSums) {
                const f32 quarterPrecipitation = quarterPrecipitationSums[index];
                if (!hasQuarterPrecipitation) {
                    driestQuarterPrecipitation = quarterPrecipitation;
                    wettestQuarterPrecipitation = quarterPrecipitation;
                    hasQuarterPrecipitation = true;
                } else {
                    driestQuarterPrecipitation = std::min(driestQuarterPrecipitation, quarterPrecipitation);
                    wettestQuarterPrecipitation = std::max(wettestQuarterPrecipitation, quarterPrecipitation);
                }
            }
        }

        climateState.completedColdestQuarterMeanTemperatureKelvin[index] = coldestQuarterTemperature;
        climateState.completedWarmestQuarterMeanTemperatureKelvin[index] = warmestQuarterTemperature;
        climateState.completedDriestQuarterPrecipitation[index] = driestQuarterPrecipitation;
        climateState.completedWettestQuarterPrecipitation[index] = wettestQuarterPrecipitation;
    }

    ++climateState.completedClimateYears;
}

void accumulateCurrentTurnMetrics(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || !climateState.turnPrecipitation || !climateState.annualPrecipitationAccumulator ||
        !climateState.currentYearTemperatureSumKelvin || !climateState.currentYearTemperatureMinKelvin ||
        !climateState.currentYearTemperatureMaxKelvin) {
        return;
    }

    const u32 quarterIndex = getQuarterIndex(climateState);
    f32* quarterTemperatureSums = climateState.currentQuarterTemperatureSumKelvin[quarterIndex].get();
    f32* quarterPrecipitationSums = climateState.currentQuarterPrecipitationAccumulator[quarterIndex].get();

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

        if (quarterTemperatureSums) {
            quarterTemperatureSums[index] += temperatureKelvin;
        }
        if (quarterPrecipitationSums) {
            quarterPrecipitationSums[index] += climateState.turnPrecipitation[index];
        }
    }

    ++climateState.currentYearTurnSamples;
    ++climateState.currentQuarterTurnSamples[quarterIndex];
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

    clearBuffer(climateState.completedAnnualPrecipitation, climateState.tileCount);
    clearBuffer(climateState.completedAnnualMeanTemperatureKelvin, climateState.tileCount);
    clearBuffer(climateState.completedAnnualTemperatureMinKelvin, climateState.tileCount);
    clearBuffer(climateState.completedAnnualTemperatureMaxKelvin, climateState.tileCount);
    clearBuffer(climateState.completedColdestQuarterMeanTemperatureKelvin, climateState.tileCount);
    clearBuffer(climateState.completedWarmestQuarterMeanTemperatureKelvin, climateState.tileCount);
    clearBuffer(climateState.completedDriestQuarterPrecipitation, climateState.tileCount);
    clearBuffer(climateState.completedWettestQuarterPrecipitation, climateState.tileCount);

    accumulateCurrentTurnMetrics(climateState);
}

void ClimateMetricsPass::advanceOneTurn(ClimateState& climateState) {
    beginNewYearIfNeeded(climateState);
    accumulateCurrentTurnMetrics(climateState);
}