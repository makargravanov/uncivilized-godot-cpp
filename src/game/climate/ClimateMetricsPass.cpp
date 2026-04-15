#include "ClimateMetricsPass.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "ClimateConfig.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

void clearBuffer(const std::unique_ptr<f32[]>& buffer, const u32 tileCount) {
    if (!buffer || tileCount == 0) {
        return;
    }

    std::memset(buffer.get(), 0, tileCount * sizeof(f32));
}

u32 getQuarterIndex(const ClimateState& climateState) {
    const u32 yearTurnCount = std::max(climateState.annualTurnCount, 1u);
    const u32 quarterIndex =
        (climateState.currentTurnIndex * CLIMATE_QUARTER_COUNT) / yearTurnCount;
    return std::min(quarterIndex, CLIMATE_QUARTER_COUNT - 1);
}

struct GlobalClimateAverages {
    f32 meanTemperatureKelvin = 0.0f;
    f32 iceFreeEquilibriumTemperatureKelvin = 0.0f;
    f32 cryosphereCoolingDeltaKelvin = 0.0f;
    f32 meanSurfaceAlbedo = 0.0f;
    f32 meanCryosphereFraction = 0.0f;
    bool valid = false;
};

f32 getRowWeight(const ClimateState& climateState, const u32 row) {
    if (climateState.zonalCellWidthMetersByRow && row < climateState.gridHeight) {
        return std::max(climateState.zonalCellWidthMetersByRow[row], 1e-3f);
    }

    return 1.0f;
}

f32 calculateCurrentTurnInsolationForRow(const ClimateState& climateState, const u32 row) {
    if (!climateState.insolationByTurnRow || climateState.annualTurnCount == 0) {
        return 0.0f;
    }

    const u32 turnIndex = climateState.currentTurnIndex % climateState.annualTurnCount;
    const f32 baseInsolation =
        climateState.insolationByTurnRow[turnIndex * climateState.gridHeight + row];
    if (!climateState.regulatorConfig.correctionEnabled) {
        return baseInsolation;
    }

    return std::max(baseInsolation + climateState.regulatorConfig.insolation.output, 0.0f);
}

f32 calculateIceFreeEquilibriumTemperatureKelvin(
    const ClimateState& climateState,
    const u32 index,
    const f32 annualMeanInsolation) {
    const f32 transmissivity = std::clamp(CONFIG.temperature.atmosphericTransmissivity, 0.0f, 1.0f);
    const f32 baseAlbedoBias =
        climateState.regulatorConfig.correctionEnabled ? climateState.regulatorConfig.baseAlbedo.output : 0.0f;
    const f32 albedo = std::clamp(climateState.baseSurfaceAlbedo[index] + baseAlbedoBias, 0.0f, 1.0f);
    const f32 absorbedShortwave = annualMeanInsolation * transmissivity * (1.0f - albedo);
    const f32 altitudeCoolingK = climateState.relativeAltitude
        ? climateState.relativeAltitude[index] * CONFIG.temperature.maxAltitudeCoolingK
        : 0.0f;
    const f32 outgoingSlope = std::max(CONFIG.temperature.outgoingLongwaveSlopeWm2PerC, 1e-3f);
    return CONFIG.shared.kelvinOffset
        + (absorbedShortwave
            - CONFIG.temperature.outgoingLongwaveBaseWm2
            - outgoingSlope * altitudeCoolingK) / outgoingSlope;
}

GlobalClimateAverages calculateGlobalClimateAverages(const ClimateState& climateState) {
    GlobalClimateAverages result;
    if (!climateState.temperatureKelvin || !climateState.surfaceAlbedo ||
        !climateState.baseSurfaceAlbedo || !climateState.relativeAltitude ||
        !climateState.snowCoverFraction || !climateState.seaIceFraction ||
        !climateState.insolationByTurnRow || climateState.gridWidth == 0 || climateState.gridHeight == 0) {
        return result;
    }

    f32 totalWeight = 0.0f;
    f32 weightedTemperatureSum = 0.0f;
    f32 weightedIceFreeEquilibriumTemperatureSum = 0.0f;
    f32 weightedAlbedoSum = 0.0f;
    f32 weightedCryosphereSum = 0.0f;

    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const f32 rowWeight = getRowWeight(climateState, row);
        const f32 rowCurrentTurnInsolation = calculateCurrentTurnInsolationForRow(climateState, row);
        const u32 rowStart = row * climateState.gridWidth;
        for (u32 column = 0; column < climateState.gridWidth; ++column) {
            const u32 index = rowStart + column;
            const f32 cryosphereFraction = std::clamp(
                std::max(climateState.snowCoverFraction[index], climateState.seaIceFraction[index]),
                0.0f,
                1.0f);
            const f32 iceFreeEquilibriumTemperatureKelvin = calculateIceFreeEquilibriumTemperatureKelvin(
                climateState,
                index,
                rowCurrentTurnInsolation);
            weightedTemperatureSum += climateState.temperatureKelvin[index] * rowWeight;
            weightedIceFreeEquilibriumTemperatureSum += iceFreeEquilibriumTemperatureKelvin * rowWeight;
            weightedAlbedoSum += climateState.surfaceAlbedo[index] * rowWeight;
            weightedCryosphereSum += cryosphereFraction * rowWeight;
            totalWeight += rowWeight;
        }
    }

    if (totalWeight <= 0.0f) {
        return result;
    }

    result.meanTemperatureKelvin = weightedTemperatureSum / totalWeight;
    result.iceFreeEquilibriumTemperatureKelvin = weightedIceFreeEquilibriumTemperatureSum / totalWeight;
    result.cryosphereCoolingDeltaKelvin =
        result.iceFreeEquilibriumTemperatureKelvin - result.meanTemperatureKelvin;
    result.meanSurfaceAlbedo = weightedAlbedoSum / totalWeight;
    result.meanCryosphereFraction = weightedCryosphereSum / totalWeight;
    result.valid = true;
    return result;
}

void publishGlobalClimateAverages(ClimateState& climateState) {
    const GlobalClimateAverages averages = calculateGlobalClimateAverages(climateState);
    if (!averages.valid) {
        climateState.currentTurnGlobalMeanTemperatureKelvin = 0.0f;
        climateState.currentTurnGlobalIceFreeEquilibriumTemperatureKelvin = 0.0f;
        climateState.currentTurnGlobalCryosphereCoolingDeltaKelvin = 0.0f;
        climateState.currentTurnGlobalMeanSurfaceAlbedo = 0.0f;
        climateState.currentTurnGlobalCryosphereFraction = 0.0f;
        return;
    }

    climateState.currentTurnGlobalMeanTemperatureKelvin = averages.meanTemperatureKelvin;
    climateState.currentTurnGlobalIceFreeEquilibriumTemperatureKelvin =
        averages.iceFreeEquilibriumTemperatureKelvin;
    climateState.currentTurnGlobalCryosphereCoolingDeltaKelvin = averages.cryosphereCoolingDeltaKelvin;
    climateState.currentTurnGlobalMeanSurfaceAlbedo = averages.meanSurfaceAlbedo;
    climateState.currentTurnGlobalCryosphereFraction = averages.meanCryosphereFraction;
}

void resetCurrentYearMetrics(ClimateState& climateState) {
    climateState.currentYearTurnSamples = 0;
    climateState.currentQuarterTurnSamples.fill(0);
    climateState.currentTurnGlobalMeanTemperatureKelvin = 0.0f;
    climateState.currentTurnGlobalIceFreeEquilibriumTemperatureKelvin = 0.0f;
    climateState.currentTurnGlobalCryosphereCoolingDeltaKelvin = 0.0f;
    climateState.currentTurnGlobalMeanSurfaceAlbedo = 0.0f;
    climateState.currentTurnGlobalCryosphereFraction = 0.0f;

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
        !climateState.surfaceAlbedo || !climateState.baseSurfaceAlbedo || !climateState.relativeAltitude ||
        !climateState.snowCoverFraction || !climateState.seaIceFraction || !climateState.insolationByTurnRow ||
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

    const f32 previousCompletedMeanTemperatureKelvin = climateState.completedGlobalMeanTemperatureKelvin;
    climateState.completedGlobalMeanTemperatureKelvin = climateState.currentTurnGlobalMeanTemperatureKelvin;
    climateState.completedGlobalIceFreeEquilibriumTemperatureKelvin =
        climateState.currentTurnGlobalIceFreeEquilibriumTemperatureKelvin;
    climateState.completedGlobalCryosphereCoolingDeltaKelvin =
        climateState.currentTurnGlobalCryosphereCoolingDeltaKelvin;
    climateState.completedGlobalMeanSurfaceAlbedo = climateState.currentTurnGlobalMeanSurfaceAlbedo;
    climateState.completedGlobalCryosphereFraction = climateState.currentTurnGlobalCryosphereFraction;
    climateState.completedGlobalMeanTemperatureDeltaKelvin = climateState.completedClimateYears > 0
        ? climateState.completedGlobalMeanTemperatureKelvin - previousCompletedMeanTemperatureKelvin
        : 0.0f;

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

    publishGlobalClimateAverages(climateState);
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
    climateState.completedGlobalMeanTemperatureKelvin = 0.0f;
    climateState.completedGlobalMeanTemperatureDeltaKelvin = 0.0f;
    climateState.completedGlobalIceFreeEquilibriumTemperatureKelvin = 0.0f;
    climateState.completedGlobalCryosphereCoolingDeltaKelvin = 0.0f;
    climateState.completedGlobalMeanSurfaceAlbedo = 0.0f;
    climateState.completedGlobalCryosphereFraction = 0.0f;

    accumulateCurrentTurnMetrics(climateState);
}

void ClimateMetricsPass::advanceOneTurn(ClimateState& climateState) {
    beginNewYearIfNeeded(climateState);
    accumulateCurrentTurnMetrics(climateState);
}
