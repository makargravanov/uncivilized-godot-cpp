#include "ClimateRegulatorPass.h"

#include <algorithm>
#include <cmath>

#include "ClimateConfig.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

f32 clampSigned(const f32 value, const f32 maxMagnitude) {
    return std::clamp(value, -maxMagnitude, maxMagnitude);
}

f32 moveTowards(const f32 current, const f32 target, const f32 maxDelta) {
    if (maxDelta <= 0.0f) {
        return target;
    }

    if (target > current) {
        return std::min(current + maxDelta, target);
    }
    return std::max(current - maxDelta, target);
}

f32 safeNormalizeSigned(const f32 value, const f32 referenceMagnitude) {
    return value / std::max(referenceMagnitude, 1e-3f);
}

void clearOutputs(ClimateState& climateState) {
    climateState.regulatorConfig.insolation.output = 0.0f;
    climateState.regulatorConfig.cryosphereAlbedo.output = 0.0f;
    climateState.regulatorConfig.baseAlbedo.output = 0.0f;
    climateState.regulatorTelemetry.heatingDemandNormalized = 0.0f;
    climateState.regulatorTelemetry.heatingDemandEquivalentWm2 = 0.0f;
}

void initializeTelemetry(ClimateState& climateState) {
    climateState.regulatorTelemetry.controllerMeanTemperatureKelvin = 0.0f;
    climateState.regulatorTelemetry.controllerTrendCPerYear = 0.0f;
    climateState.regulatorTelemetry.controllerCryosphereCoolingDeltaKelvin = 0.0f;
    climateState.regulatorTelemetry.temperatureErrorC = 0.0f;
    climateState.regulatorTelemetry.gains.kp = CONFIG.regulator.baseProportionalGain;
    climateState.regulatorTelemetry.gains.kd = CONFIG.regulator.baseDifferentialGain;
    climateState.regulatorTelemetry.gains.kff = CONFIG.regulator.baseFeedForwardGain;
    climateState.regulatorPreviousTemperatureErrorC = 0.0f;
    climateState.regulatorPreviousControllerMeanTemperatureKelvin = 0.0f;
    climateState.regulatorHasPreviousControllerMean = false;
    climateState.regulatorHistoryCursor = 0;
    climateState.regulatorHistoryCount = 0;
    climateState.regulatorRollingTemperatureSumKelvin = 0.0f;
    climateState.regulatorRollingCryosphereCoolingDeltaSumKelvin = 0.0f;
    clearOutputs(climateState);
}

void pushRollingHistorySample(ClimateState& climateState, const f32 meanTemperatureKelvin, const f32 cryosphereDeltaKelvin) {
    if (!climateState.regulatorTurnMeanTemperatureHistoryKelvin ||
        !climateState.regulatorTurnCryosphereCoolingDeltaHistoryKelvin ||
        climateState.annualTurnCount == 0) {
        return;
    }

    const u32 windowSize = climateState.annualTurnCount;
    if (climateState.regulatorHistoryCount == windowSize) {
        climateState.regulatorRollingTemperatureSumKelvin -=
            climateState.regulatorTurnMeanTemperatureHistoryKelvin[climateState.regulatorHistoryCursor];
        climateState.regulatorRollingCryosphereCoolingDeltaSumKelvin -=
            climateState.regulatorTurnCryosphereCoolingDeltaHistoryKelvin[climateState.regulatorHistoryCursor];
    } else {
        ++climateState.regulatorHistoryCount;
    }

    climateState.regulatorTurnMeanTemperatureHistoryKelvin[climateState.regulatorHistoryCursor] = meanTemperatureKelvin;
    climateState.regulatorTurnCryosphereCoolingDeltaHistoryKelvin[climateState.regulatorHistoryCursor] = cryosphereDeltaKelvin;
    climateState.regulatorRollingTemperatureSumKelvin += meanTemperatureKelvin;
    climateState.regulatorRollingCryosphereCoolingDeltaSumKelvin += cryosphereDeltaKelvin;
    climateState.regulatorHistoryCursor = (climateState.regulatorHistoryCursor + 1) % windowSize;
}

void updateTelemetryFromRollingHistory(ClimateState& climateState) {
    if (climateState.regulatorHistoryCount == 0) {
        climateState.regulatorTelemetry.controllerMeanTemperatureKelvin = 0.0f;
        climateState.regulatorTelemetry.controllerCryosphereCoolingDeltaKelvin = 0.0f;
        climateState.regulatorTelemetry.controllerTrendCPerYear = 0.0f;
        climateState.regulatorTelemetry.temperatureErrorC = 0.0f;
        clearOutputs(climateState);
        return;
    }

    const f32 inverseHistoryCount = 1.0f / static_cast<f32>(climateState.regulatorHistoryCount);
    const f32 controllerMeanTemperatureKelvin = climateState.regulatorRollingTemperatureSumKelvin * inverseHistoryCount;
    const f32 controllerCryosphereDeltaKelvin =
        climateState.regulatorRollingCryosphereCoolingDeltaSumKelvin * inverseHistoryCount;
    const f32 controllerTrendCPerYear = climateState.regulatorHasPreviousControllerMean
        ? (controllerMeanTemperatureKelvin - climateState.regulatorPreviousControllerMeanTemperatureKelvin)
            * static_cast<f32>(std::max(climateState.annualTurnCount, 1u))
        : 0.0f;

    climateState.regulatorTelemetry.controllerMeanTemperatureKelvin = controllerMeanTemperatureKelvin;
    climateState.regulatorTelemetry.controllerCryosphereCoolingDeltaKelvin = controllerCryosphereDeltaKelvin;
    climateState.regulatorTelemetry.controllerTrendCPerYear = controllerTrendCPerYear;
    climateState.regulatorTelemetry.temperatureErrorC =
        climateState.regulatorConfig.targetGlobalMeanTemperatureC
        - (controllerMeanTemperatureKelvin - CONFIG.shared.kelvinOffset);

    climateState.regulatorPreviousControllerMeanTemperatureKelvin = controllerMeanTemperatureKelvin;
    climateState.regulatorHasPreviousControllerMean = true;
}

void adaptGains(ClimateState& climateState) {
    ClimateRegulatorAdaptiveGainRuntime& gains = climateState.regulatorTelemetry.gains;
    const f32 errorC = climateState.regulatorTelemetry.temperatureErrorC;
    const f32 trendCPerYear = climateState.regulatorTelemetry.controllerTrendCPerYear;
    const f32 cryosphereDeltaC = climateState.regulatorTelemetry.controllerCryosphereCoolingDeltaKelvin;
    const f32 previousErrorC = climateState.regulatorPreviousTemperatureErrorC;

    const f32 errorMagnitude = std::clamp(
        std::abs(errorC) / std::max(CONFIG.regulator.errorReferenceC, 1e-3f),
        0.0f,
        1.0f);
    const f32 approachMagnitude = errorC * trendCPerYear > 0.0f
        ? std::clamp(std::abs(trendCPerYear) / std::max(CONFIG.regulator.trendReferenceCPerYear, 1e-3f), 0.0f, 1.0f)
        : 0.0f;
    const f32 cryosphereMagnitude = std::clamp(
        std::abs(cryosphereDeltaC) / std::max(CONFIG.regulator.cryosphereReferenceC, 1e-3f),
        0.0f,
        1.0f);
    const bool oscillating =
        previousErrorC != 0.0f &&
        ((previousErrorC > 0.0f && errorC < 0.0f) || (previousErrorC < 0.0f && errorC > 0.0f));

    f32 targetKp = CONFIG.regulator.minProportionalGain +
        (CONFIG.regulator.maxProportionalGain - CONFIG.regulator.minProportionalGain) * errorMagnitude;
    if (approachMagnitude > 0.0f) {
        targetKp = std::max(
            CONFIG.regulator.minProportionalGain,
            targetKp - approachMagnitude * 0.35f * (CONFIG.regulator.maxProportionalGain - CONFIG.regulator.minProportionalGain));
    }

    f32 targetKd = CONFIG.regulator.baseDifferentialGain +
        approachMagnitude * (CONFIG.regulator.maxDifferentialGain - CONFIG.regulator.baseDifferentialGain);
    if (oscillating) {
        targetKd = std::min(
            CONFIG.regulator.maxDifferentialGain,
            targetKd + 0.25f * (CONFIG.regulator.maxDifferentialGain - CONFIG.regulator.minDifferentialGain));
    }
    if (errorMagnitude > 0.9f && approachMagnitude < 0.1f) {
        targetKd = std::max(CONFIG.regulator.minDifferentialGain, targetKd * 0.6f);
    }

    const f32 targetKff = CONFIG.regulator.minFeedForwardGain +
        cryosphereMagnitude * (CONFIG.regulator.maxFeedForwardGain - CONFIG.regulator.minFeedForwardGain);

    gains.kp = moveTowards(
        gains.kp,
        std::clamp(targetKp, CONFIG.regulator.minProportionalGain, CONFIG.regulator.maxProportionalGain),
        CONFIG.regulator.proportionalGainAdaptationRate);
    gains.kd = moveTowards(
        gains.kd,
        std::clamp(targetKd, CONFIG.regulator.minDifferentialGain, CONFIG.regulator.maxDifferentialGain),
        CONFIG.regulator.differentialGainAdaptationRate);
    gains.kff = moveTowards(
        gains.kff,
        std::clamp(targetKff, CONFIG.regulator.minFeedForwardGain, CONFIG.regulator.maxFeedForwardGain),
        CONFIG.regulator.feedForwardGainAdaptationRate);

    climateState.regulatorPreviousTemperatureErrorC = errorC;
}

f32 computeHeatingDemand(ClimateState& climateState) {
    adaptGains(climateState);

    const ClimateRegulatorAdaptiveGainRuntime& gains = climateState.regulatorTelemetry.gains;
    const f32 errorSignal = safeNormalizeSigned(
        climateState.regulatorTelemetry.temperatureErrorC,
        CONFIG.regulator.errorReferenceC);
    const f32 trendSignal = safeNormalizeSigned(
        climateState.regulatorTelemetry.controllerTrendCPerYear,
        CONFIG.regulator.trendReferenceCPerYear);
    const f32 cryosphereSignal = safeNormalizeSigned(
        climateState.regulatorTelemetry.controllerCryosphereCoolingDeltaKelvin,
        CONFIG.regulator.cryosphereReferenceC);

    const f32 rawDemand =
        gains.kp * errorSignal
        - gains.kd * trendSignal
        + gains.kff * cryosphereSignal;
    const f32 clampedDemand = std::clamp(rawDemand, -1.0f, 1.0f);
    const f32 previousDemand = climateState.regulatorTelemetry.heatingDemandNormalized;
    const f32 demand = moveTowards(previousDemand, clampedDemand, CONFIG.regulator.demandSlewPerTurn);

    climateState.regulatorTelemetry.heatingDemandNormalized = demand;
    climateState.regulatorTelemetry.heatingDemandEquivalentWm2 =
        demand * CONFIG.regulator.demandReferenceWm2;
    return demand;
}

f32 resolveActuatorTarget(
    const ClimateState& climateState,
    const ClimateRegulatorActuatorRuntime& actuator,
    const f32 demand,
    const f32 signMultiplier) {
    if (!climateState.regulatorConfig.correctionEnabled || !actuator.enabled) {
        return 0.0f;
    }

    const f32 scaledTarget = signMultiplier * demand * actuator.strength * actuator.maxMagnitude;
    return clampSigned(scaledTarget, actuator.maxMagnitude);
}

void applyActuatorTargets(ClimateState& climateState, const f32 demand) {
    ClimateRegulatorActuatorRuntime& insolation = climateState.regulatorConfig.insolation;
    ClimateRegulatorActuatorRuntime& cryosphere = climateState.regulatorConfig.cryosphereAlbedo;
    ClimateRegulatorActuatorRuntime& baseAlbedo = climateState.regulatorConfig.baseAlbedo;

    const f32 insolationTarget = resolveActuatorTarget(climateState, insolation, demand, 1.0f);
    const f32 cryosphereTarget = resolveActuatorTarget(climateState, cryosphere, demand, -1.0f);
    const f32 baseAlbedoTarget = resolveActuatorTarget(climateState, baseAlbedo, demand, -1.0f);

    insolation.output = climateState.regulatorConfig.correctionEnabled && insolation.enabled
        ? moveTowards(insolation.output, insolationTarget, CONFIG.regulator.insolationOutputSlewPerTurn)
        : 0.0f;
    cryosphere.output = climateState.regulatorConfig.correctionEnabled && cryosphere.enabled
        ? moveTowards(cryosphere.output, cryosphereTarget, CONFIG.regulator.cryosphereAlbedoOutputSlewPerTurn)
        : 0.0f;
    baseAlbedo.output = climateState.regulatorConfig.correctionEnabled && baseAlbedo.enabled
        ? moveTowards(baseAlbedo.output, baseAlbedoTarget, CONFIG.regulator.baseAlbedoOutputSlewPerTurn)
        : 0.0f;
}

void updateRegulator(ClimateState& climateState) {
    if (climateState.gridHeight == 0 || climateState.gridWidth == 0 || climateState.annualTurnCount == 0) {
        initializeTelemetry(climateState);
        return;
    }

    pushRollingHistorySample(
        climateState,
        climateState.currentTurnGlobalMeanTemperatureKelvin,
        climateState.currentTurnGlobalCryosphereCoolingDeltaKelvin);
    updateTelemetryFromRollingHistory(climateState);
    const f32 demand = computeHeatingDemand(climateState);
    applyActuatorTargets(climateState, demand);
}

} // namespace

void ClimateRegulatorPass::initialize(ClimateState& climateState) {
    initializeTelemetry(climateState);
    updateRegulator(climateState);
}

void ClimateRegulatorPass::advanceOneTurn(ClimateState& climateState) {
    updateRegulator(climateState);
}
