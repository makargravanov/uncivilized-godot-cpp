#include "ClimateRegulatorPass.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "ClimateConfig.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;
constexpr f32 HALF_PI = 1.5707963267948966f;

f32 getRowWeight(const ClimateState& climateState, const u32 row) {
    if (climateState.zonalCellWidthMetersByRow && row < climateState.gridHeight) {
        return std::max(climateState.zonalCellWidthMetersByRow[row], 1e-3f);
    }

    return 1.0f;
}

f32 getRowLatitudeShape(const ClimateState& climateState, const u32 row) {
    const f32 exponent = std::max(CONFIG.regulator.latitudeShapeExponent, 0.1f);
    f32 normalizedLatitude = 0.0f;

    if (climateState.latitudeRadians && row < climateState.gridHeight) {
        const u32 rowIndex = row * climateState.gridWidth;
        normalizedLatitude = std::clamp(std::abs(climateState.latitudeRadians[rowIndex]) / HALF_PI, 0.0f, 1.0f);
    } else if (climateState.gridHeight > 0) {
        const f32 normalizedRow = (static_cast<f32>(row) + 0.5f) / static_cast<f32>(climateState.gridHeight);
        normalizedLatitude = std::clamp(std::abs(normalizedRow - 0.5f) * 2.0f, 0.0f, 1.0f);
    }

    return std::pow(normalizedLatitude, exponent);
}

void clearRegulatorState(ClimateState& climateState) {
    climateState.currentYearRegulatorTargetTemperatureC = CONFIG.regulator.targetGlobalMeanTemperatureC;
    climateState.currentYearRegulatorTemperatureErrorC = 0.0f;
    climateState.currentYearRegulatorTrendCPerYear = 0.0f;
    climateState.currentYearRegulatorCryosphereCoolingDeltaC = 0.0f;
    climateState.currentYearRegulatorControlSignalWm2 = 0.0f;
    climateState.currentYearRegulatorRowBiasMinWm2 = 0.0f;
    climateState.currentYearRegulatorRowBiasMaxWm2 = 0.0f;
    climateState.currentYearRegulatorRowBiasMeanAbsWm2 = 0.0f;

    if (climateState.currentYearRegulatorInsolationBiasWm2ByRow && climateState.gridHeight > 0) {
        std::memset(
            climateState.currentYearRegulatorInsolationBiasWm2ByRow.get(),
            0,
            climateState.gridHeight * sizeof(f32));
    }
}

void computeRegulator(ClimateState& climateState) {
    if (!climateState.currentYearRegulatorInsolationBiasWm2ByRow || climateState.gridHeight == 0 ||
        climateState.gridWidth == 0 || climateState.currentYearTurnSamples == 0) {
        clearRegulatorState(climateState);
        return;
    }

    climateState.currentYearRegulatorTargetTemperatureC = CONFIG.regulator.targetGlobalMeanTemperatureC;
    const f32 currentTemperatureC =
        climateState.currentYearGlobalMeanTemperatureKelvin - CONFIG.shared.kelvinOffset;
    const f32 temperatureErrorC =
        climateState.currentYearRegulatorTargetTemperatureC - currentTemperatureC;
    const f32 trendCPerYear = climateState.completedClimateYears > 0
        ? climateState.completedGlobalMeanTemperatureDeltaKelvin
        : 0.0f;
    const f32 cryosphereCoolingDeltaC = climateState.currentYearGlobalCryosphereCoolingDeltaKelvin;

    f32 controlSignalWm2 =
        temperatureErrorC * CONFIG.regulator.temperatureErrorGainWm2PerC
        - trendCPerYear * CONFIG.regulator.temperatureTrendGainWm2PerCPerYear
        + cryosphereCoolingDeltaC * CONFIG.regulator.cryosphereFeedForwardGainWm2PerC;
    controlSignalWm2 = std::clamp(
        controlSignalWm2,
        -CONFIG.regulator.maxInsolationCorrectionWm2,
        CONFIG.regulator.maxInsolationCorrectionWm2);

    f32 weightedShapeSum = 0.0f;
    f32 totalWeight = 0.0f;
    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const f32 rowWeight = getRowWeight(climateState, row);
        const f32 rowShape = getRowLatitudeShape(climateState, row);
        climateState.currentYearRegulatorInsolationBiasWm2ByRow[row] = rowShape;
        weightedShapeSum += rowShape * rowWeight;
        totalWeight += rowWeight;
    }

    const f32 meanShape = totalWeight > 0.0f ? weightedShapeSum / totalWeight : 0.0f;
    f32 minBias = std::numeric_limits<f32>::infinity();
    f32 maxBias = -std::numeric_limits<f32>::infinity();
    f32 weightedAbsoluteBiasSum = 0.0f;

    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const f32 rowWeight = getRowWeight(climateState, row);
        const f32 centeredShape = climateState.currentYearRegulatorInsolationBiasWm2ByRow[row] - meanShape;
        const f32 rowBiasWm2 = controlSignalWm2 * centeredShape;
        climateState.currentYearRegulatorInsolationBiasWm2ByRow[row] = rowBiasWm2;
        minBias = std::min(minBias, rowBiasWm2);
        maxBias = std::max(maxBias, rowBiasWm2);
        weightedAbsoluteBiasSum += std::abs(rowBiasWm2) * rowWeight;
    }

    climateState.currentYearRegulatorTemperatureErrorC = temperatureErrorC;
    climateState.currentYearRegulatorTrendCPerYear = trendCPerYear;
    climateState.currentYearRegulatorCryosphereCoolingDeltaC = cryosphereCoolingDeltaC;
    climateState.currentYearRegulatorControlSignalWm2 = controlSignalWm2;
    climateState.currentYearRegulatorRowBiasMinWm2 = std::isfinite(minBias) ? minBias : 0.0f;
    climateState.currentYearRegulatorRowBiasMaxWm2 = std::isfinite(maxBias) ? maxBias : 0.0f;
    climateState.currentYearRegulatorRowBiasMeanAbsWm2 =
        totalWeight > 0.0f ? weightedAbsoluteBiasSum / totalWeight : 0.0f;
}

} // namespace

void ClimateRegulatorPass::initialize(ClimateState& climateState) {
    computeRegulator(climateState);
}

void ClimateRegulatorPass::advanceOneTurn(ClimateState& climateState) {
    computeRegulator(climateState);
}
