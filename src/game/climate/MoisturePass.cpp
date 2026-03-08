#include "MoisturePass.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "ClimateConfig.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

// ─── helpers ───────────────────────────────────────────────────────

f32 calculateSaturationVaporPressureKpa(f32 temperatureCelsius) {
    return CONFIG.shared.magnusBasePressureKpa * std::exp(
        CONFIG.shared.magnusCoefficientA * temperatureCelsius
        / (temperatureCelsius + CONFIG.shared.magnusCoefficientB));
}

f32 calculateMaxSpecificHumidity(f32 temperatureCelsius) {
    const f32 saturationPressure = calculateSaturationVaporPressureKpa(temperatureCelsius);
    return CONFIG.shared.waterToDryAirMolecularRatio * saturationPressure
        / (CONFIG.shared.standardAtmosphericPressureKpa - saturationPressure);
}

bool isOceanTile(const ClimateState& climateState, u32 index) {
    return climateState.relativeAltitude[index] < CONFIG.shared.oceanAltitudeThreshold;
}

u32 wrapColumn(i32 column, u32 width) {
    const i32 wrapped = column % static_cast<i32>(width);
    return static_cast<u32>(wrapped < 0 ? wrapped + static_cast<i32>(width) : wrapped);
}

u32 clampRow(i32 row, u32 height) {
    return static_cast<u32>(std::clamp(row, 0, static_cast<i32>(height - 1)));
}

u32 tileIndex(const ClimateState& cs, i32 column, i32 row) {
    return clampRow(row, cs.gridHeight) * cs.gridWidth + wrapColumn(column, cs.gridWidth);
}

f32 sampleRelativeAltitude(const ClimateState& climateState, const i32 column, const i32 row) {
    return climateState.relativeAltitude[tileIndex(climateState, column, row)];
}

f32 computeAltitudeGradientX(const ClimateState& climateState, const u32 column, const u32 row) {
    const f32 leftHeight = sampleRelativeAltitude(
        climateState, static_cast<i32>(column) - 1, static_cast<i32>(row));
    const f32 rightHeight = sampleRelativeAltitude(
        climateState, static_cast<i32>(column) + 1, static_cast<i32>(row));
    return (rightHeight - leftHeight) / (2.0f * CONFIG.shared.tileStepX);
}

f32 computeAltitudeGradientZ(const ClimateState& climateState, const u32 column, const u32 row) {
    const i32 signedColumn = static_cast<i32>(column);
    const i32 signedRow = static_cast<i32>(row);

    if (row == 0) {
        const f32 currentHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow);
        const f32 nextHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow + 1);
        return (nextHeight - currentHeight) / CONFIG.shared.tileStepZ;
    }

    if (row + 1 >= climateState.gridHeight) {
        const f32 currentHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow);
        const f32 previousHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow - 1);
        return (currentHeight - previousHeight) / CONFIG.shared.tileStepZ;
    }

    const f32 previousHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow - 1);
    const f32 nextHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow + 1);
    return (nextHeight - previousHeight) / (2.0f * CONFIG.shared.tileStepZ);
}

f32 computeOrographicLiftCoolingK(
    const ClimateState& climateState,
    const u32 column,
    const u32 row,
    const f32 windEast,
    const f32 windNorth,
    const f32 windSpeed) {
    const f32 gradientEast = computeAltitudeGradientX(climateState, column, row);
    // computeAltitudeGradientZ returns gradient in +row (south) direction;
    // negate to get the northward gradient matching windNorth convention.
    const f32 gradientNorth = -computeAltitudeGradientZ(climateState, column, row);
    const f32 directionEast = windEast / windSpeed;
    const f32 directionNorth = windNorth / windSpeed;
    const f32 upslope = std::max(
        0.0f,
        gradientEast * directionEast + gradientNorth * directionNorth);
    const f32 windSpeedFactor = std::clamp(
        windSpeed / CONFIG.moisture.referenceAdvectionWindSpeedMps,
        0.0f,
        1.5f);

    return std::clamp(
        upslope * CONFIG.moisture.orographicLiftCoolingPerUnitUpslopeK * windSpeedFactor,
        0.0f,
        CONFIG.moisture.maxOrographicLiftCoolingK);
}

// ─── physics steps ─────────────────────────────────────────────────

void applyOceanEvaporation(ClimateState& climateState) {
    for (u32 i = 0; i < climateState.tileCount; ++i) {
        if (!isOceanTile(climateState, i)) continue;

        const f32 temperatureCelsius = climateState.temperatureKelvin[i] - CONFIG.shared.kelvinOffset;
        const f32 maxHumidity = calculateMaxSpecificHumidity(temperatureCelsius);
        const f32 currentHumidity = climateState.humidityKgPerKg[i];
        const f32 humidityDeficit = std::max(0.0f, maxHumidity - currentHumidity);

        climateState.humidityKgPerKg[i] += humidityDeficit
            * CONFIG.moisture.oceanEvaporationRate
            * CONFIG.moisture.oceanEvaporationWindFactor;
        climateState.humidityKgPerKg[i] = std::min(climateState.humidityKgPerKg[i], maxHumidity);
    }
}

void precomputeTransportMetadata(ClimateState& climateState) {
    if (!climateState.windEastMps || !climateState.windNorthMps ||
        !climateState.moistureUpwindEastIndex || !climateState.moistureUpwindNorthIndex ||
        !climateState.moistureEastWeight || !climateState.moistureNorthWeight ||
        !climateState.moistureMixingFactor || !climateState.moistureOrographicCoolingK ||
        !climateState.relativeAltitude) {
        return;
    }

    const u32 width = climateState.gridWidth;
    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        for (u32 col = 0; col < width; ++col) {
            const u32 idx = row * width + col;
            const f32 windEast = climateState.windEastMps[idx];
            const f32 windNorth = climateState.windNorthMps[idx];
            const f32 windSpeed = std::sqrt(windEast * windEast + windNorth * windNorth);

            if (windSpeed < CONFIG.shared.minWindSpeedMps) {
                climateState.moistureUpwindEastIndex[idx] = idx;
                climateState.moistureUpwindNorthIndex[idx] = idx;
                climateState.moistureEastWeight[idx] = 1.0f;
                climateState.moistureNorthWeight[idx] = 0.0f;
                climateState.moistureMixingFactor[idx] = 0.0f;
                climateState.moistureOrographicCoolingK[idx] = 0.0f;
                continue;
            }

            const i32 upwindCol = windEast > 0.0f
                ? static_cast<i32>(col) - 1
                : static_cast<i32>(col) + 1;
            const i32 upwindRow = windNorth > 0.0f
                ? static_cast<i32>(row) + 1
                : static_cast<i32>(row) - 1;

            climateState.moistureUpwindEastIndex[idx] =
                tileIndex(climateState, upwindCol, static_cast<i32>(row));
            climateState.moistureUpwindNorthIndex[idx] =
                tileIndex(climateState, static_cast<i32>(col), upwindRow);
            climateState.moistureEastWeight[idx] = std::abs(windEast) / windSpeed;
            climateState.moistureNorthWeight[idx] = std::abs(windNorth) / windSpeed;
            climateState.moistureMixingFactor[idx] = std::clamp(
                CONFIG.moisture.advectionMixingFraction
                    * (windSpeed / CONFIG.moisture.referenceAdvectionWindSpeedMps),
                0.0f,
                CONFIG.moisture.maxAdvectionMixingFactor);
            climateState.moistureOrographicCoolingK[idx] = computeOrographicLiftCoolingK(
                climateState,
                col,
                row,
                windEast,
                windNorth,
                windSpeed);
        }
    }
}

void advectMoistureOneSubStep(ClimateState& climateState) {
    if (!climateState.humidityScratchKgPerKg || !climateState.humidityKgPerKg ||
        !climateState.moistureUpwindEastIndex || !climateState.moistureUpwindNorthIndex ||
        !climateState.moistureEastWeight || !climateState.moistureNorthWeight ||
        !climateState.moistureMixingFactor) {
        return;
    }

    const f32* previousHumidity = climateState.humidityScratchKgPerKg.get();
    f32* currentHumidity = climateState.humidityKgPerKg.get();
    const u32* upwindEastIndex = climateState.moistureUpwindEastIndex.get();
    const u32* upwindNorthIndex = climateState.moistureUpwindNorthIndex.get();
    const f32* eastWeight = climateState.moistureEastWeight.get();
    const f32* northWeight = climateState.moistureNorthWeight.get();
    const f32* mixingFactor = climateState.moistureMixingFactor.get();

    for (u32 idx = 0; idx < climateState.tileCount; ++idx) {
        const f32 upwindBlended =
            eastWeight[idx] * previousHumidity[upwindEastIndex[idx]]
            + northWeight[idx] * previousHumidity[upwindNorthIndex[idx]];
        const f32 humidityHere = previousHumidity[idx];
        currentHumidity[idx] = humidityHere + (upwindBlended - humidityHere) * mixingFactor[idx];
    }
}

void advectMoisture(ClimateState& climateState) {
    if (!climateState.humidityScratchKgPerKg || !climateState.humidityKgPerKg) {
        return;
    }

    for (u32 step = 0; step < CONFIG.moisture.advectionSubSteps; ++step) {
        std::memcpy(climateState.humidityScratchKgPerKg.get(), climateState.humidityKgPerKg.get(),
                    climateState.tileCount * sizeof(f32));
        advectMoistureOneSubStep(climateState);
    }
}

void applyCondensationAndPrecipitation(ClimateState& climateState) {
    const f32* orographicCoolingK = climateState.moistureOrographicCoolingK.get();

    for (u32 i = 0; i < climateState.tileCount; ++i) {
        const f32 temperatureCelsius = climateState.temperatureKelvin[i] - CONFIG.shared.kelvinOffset;
        const f32 maxHumidity = calculateMaxSpecificHumidity(temperatureCelsius);

        f32 currentHumidity = climateState.humidityKgPerKg[i];
        const f32 ambientExcess = std::max(0.0f, currentHumidity - maxHumidity);
        if (ambientExcess > 0.0f) {
            currentHumidity = maxHumidity;
            climateState.turnPrecipitation[i] += ambientExcess;
        }

        if (orographicCoolingK && orographicCoolingK[i] > 0.0f && currentHumidity > 0.0f) {
            const f32 liftedTemperatureCelsius = temperatureCelsius - orographicCoolingK[i];
            const f32 liftedMaxHumidity = calculateMaxSpecificHumidity(liftedTemperatureCelsius);
            const f32 orographicCondensation = std::max(0.0f, currentHumidity - liftedMaxHumidity)
                * CONFIG.moisture.orographicPrecipitationEfficiency;

            currentHumidity = std::max(0.0f, currentHumidity - orographicCondensation);
            climateState.turnPrecipitation[i] += orographicCondensation;
        }

        climateState.humidityKgPerKg[i] = currentHumidity;
    }
}

} // namespace

void MoisturePass::initialize(ClimateState& climateState) {
    if (!climateState.humidityKgPerKg || !climateState.temperatureKelvin) return;

    for (u32 i = 0; i < climateState.tileCount; ++i) {
        const f32 temperatureCelsius = climateState.temperatureKelvin[i] - CONFIG.shared.kelvinOffset;
        const f32 maxHumidity = calculateMaxSpecificHumidity(temperatureCelsius);

        const f32 fraction = isOceanTile(climateState, i)
            ? CONFIG.moisture.oceanInitialHumidityFraction
            : CONFIG.moisture.landInitialHumidityFraction;

        climateState.humidityKgPerKg[i] = maxHumidity * fraction;
    }

    std::memset(climateState.turnPrecipitation.get(), 0, climateState.tileCount * sizeof(f32));
    precomputeTransportMetadata(climateState);

    // Run several cycles to reach a plausible initial distribution.
    for (u32 warmup = 0; warmup < 4; ++warmup) {
        applyOceanEvaporation(climateState);
        advectMoisture(climateState);
        applyCondensationAndPrecipitation(climateState);
    }

    // Clear warmup artifacts, then compute one clean initial precipitation snapshot.
    std::memset(climateState.turnPrecipitation.get(), 0, climateState.tileCount * sizeof(f32));

    applyOceanEvaporation(climateState);
    advectMoisture(climateState);
    applyCondensationAndPrecipitation(climateState);
}

void MoisturePass::advanceOneTurn(ClimateState& climateState) {
    if (!climateState.humidityKgPerKg || !climateState.temperatureKelvin ||
        !climateState.windEastMps || !climateState.windNorthMps) return;

    std::memset(climateState.turnPrecipitation.get(), 0, climateState.tileCount * sizeof(f32));
    precomputeTransportMetadata(climateState);

    applyOceanEvaporation(climateState);
    advectMoisture(climateState);
    applyCondensationAndPrecipitation(climateState);
}

void MoisturePass::publishToTiles(const ClimateState& climateState, const std::unique_ptr<TileData[]>& tiles) {
    if (!tiles || !climateState.humidityKgPerKg) return;

    for (u32 i = 0; i < climateState.tileCount; ++i) {
        // Normalize to 0..255 for compact TileData storage.
        const f32 normalized = std::clamp(
            climateState.humidityKgPerKg[i] / CONFIG.moisture.maxOverlayHumidity, 0.0f, 1.0f);
        tiles[i].humidity = static_cast<u8>(normalized * 255.0f);
    }
}

f32 MoisturePass::normalizeForOverlay(f32 humidityKgPerKg) {
    return std::clamp(
        (humidityKgPerKg - CONFIG.moisture.minOverlayHumidity)
            / (CONFIG.moisture.maxOverlayHumidity - CONFIG.moisture.minOverlayHumidity),
        0.0f, 1.0f);
}

f32 MoisturePass::normalizePrecipitationForOverlay(f32 turnPrecipitation) {
    const f32 linear = std::clamp(
        (turnPrecipitation - CONFIG.moisture.minOverlayTurnPrecipitation)
            / (CONFIG.moisture.maxOverlayTurnPrecipitation
                - CONFIG.moisture.minOverlayTurnPrecipitation),
        0.0f,
        1.0f);

    return std::pow(linear, CONFIG.moisture.precipitationOverlayResponseExponent);
}
