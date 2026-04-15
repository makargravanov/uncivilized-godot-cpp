#include "ClimateRegulatorPass.h"

#include <algorithm>

#include "ClimateConfig.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

void clearRegulatorState(ClimateState& climateState) {
    climateState.currentYearRegulatorTargetTemperatureC = climateState.regulatorTargetGlobalMeanTemperatureC;
    climateState.currentYearRegulatorTemperatureErrorC = 0.0f;
    climateState.currentYearRegulatorTrendCPerYear = 0.0f;
    climateState.currentYearRegulatorCryosphereCoolingDeltaC = 0.0f;
    climateState.currentYearRegulatorControlSignalWm2 = 0.0f;
}

void computeRegulator(ClimateState& climateState) {
    if (climateState.gridHeight == 0 || climateState.gridWidth == 0 || climateState.currentYearTurnSamples == 0) {
        clearRegulatorState(climateState);
        return;
    }

    climateState.currentYearRegulatorTargetTemperatureC = climateState.regulatorTargetGlobalMeanTemperatureC;

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

    climateState.currentYearRegulatorTemperatureErrorC = temperatureErrorC;
    climateState.currentYearRegulatorTrendCPerYear = trendCPerYear;
    climateState.currentYearRegulatorCryosphereCoolingDeltaC = cryosphereCoolingDeltaC;
    climateState.currentYearRegulatorControlSignalWm2 = controlSignalWm2;
}

} // namespace

void ClimateRegulatorPass::initialize(ClimateState& climateState) {
    computeRegulator(climateState);
}

void ClimateRegulatorPass::advanceOneTurn(ClimateState& climateState) {
    computeRegulator(climateState);
}
