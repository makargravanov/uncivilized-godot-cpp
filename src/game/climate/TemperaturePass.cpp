//
// Created by Copilot on 06.03.2026.
//

#include "TemperaturePass.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Astrophysics.h"
#include "ClimateConfig.h"
#include "game/map/elevations-creation/PlatecWrapper.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;
constexpr Astro::StarParams STAR_PARAMS;
constexpr Astro::OrbitalParams ORBITAL_PARAMS;
constexpr f32 MAX_DIFFUSIVE_COURANT_NUMBER = 0.45f;

u32 lookupIndex(const ClimateState& climateState, const u32 turnIndex, const u32 row) {
    return turnIndex * climateState.gridHeight + row;
}

u32 wrapColumn(const i32 column, const u32 width) {
    const i32 wrapped = column % static_cast<i32>(width);
    return static_cast<u32>(wrapped < 0 ? wrapped + static_cast<i32>(width) : wrapped);
}

u32 flattenIndex(const ClimateState& climateState, const u32 row, const u32 column) {
    return row * climateState.gridWidth + column;
}

bool isOceanCell(const ClimateState& climateState, const u32 index) {
    return climateState.relativeAltitude[index] < CONFIG.shared.oceanAltitudeThreshold;
}

f32 sampleTemperatureC(const f32* temperatureKelvin, const u32 index) {
    return temperatureKelvin[index] - CONFIG.shared.kelvinOffset;
}

f32 getPlanetRadiusMeters() {
    return std::max(CONFIG.temperature.planetRadiusMeters, 1.0e3f);
}

f32 getEquatorialCellWidthMeters(const ClimateState& climateState) {
    return static_cast<f32>((Astro::TWO_PI * getPlanetRadiusMeters()) / static_cast<f64>(climateState.gridWidth));
}

f32 getMeridionalCellHeightMeters(const ClimateState& climateState) {
    return static_cast<f32>((Astro::PI * getPlanetRadiusMeters()) / static_cast<f64>(climateState.gridHeight));
}

f32 getZonalCellWidthMeters(const ClimateState& climateState, const u32 row) {
    const u32 rowIndex = flattenIndex(climateState, row, 0);
    const f32 latitudeRadians = climateState.latitudeRadians
        ? climateState.latitudeRadians[rowIndex]
        : 0.0f;
    const f32 zonalWidthFactor = std::max(
        std::cos(latitudeRadians),
        CONFIG.temperature.minimumZonalCellWidthFactor);
    return getEquatorialCellWidthMeters(climateState) * zonalWidthFactor;
}

f32 getContactThermalDiffusivity(const ClimateState& climateState, const u32 firstIndex, const u32 secondIndex) {
    const bool firstOcean = isOceanCell(climateState, firstIndex);
    const bool secondOcean = isOceanCell(climateState, secondIndex);
    if (firstOcean && secondOcean) {
        return CONFIG.temperature.oceanThermalDiffusivityM2PerS;
    }
    if (firstOcean != secondOcean) {
        return CONFIG.temperature.coastalThermalDiffusivityM2PerS;
    }
    return CONFIG.temperature.landThermalDiffusivityM2PerS;
}

f32 calculateAbsorbedShortwave(const f32 insolationWm2, const f32 surfaceAlbedo) {
    const f32 transmissivity = std::clamp(CONFIG.temperature.atmosphericTransmissivity, 0.0f, 1.0f);
    const f32 albedo = std::clamp(surfaceAlbedo, 0.0f, 1.0f);
    return insolationWm2 * transmissivity * (1.0f - albedo);
}

f32 calculateAltitudeCoolingK(const ClimateState& climateState, const u32 index) {
    return climateState.relativeAltitude[index] * CONFIG.temperature.maxAltitudeCoolingK;
}

f32 calculateRadiativeEquilibriumTemperatureC(const f32 absorbedShortwaveWm2, const f32 altitudeCoolingK) {
    const f32 outgoingSlope = std::max(CONFIG.temperature.outgoingLongwaveSlopeWm2PerC, 1e-3f);
    return (absorbedShortwaveWm2
        - CONFIG.temperature.outgoingLongwaveBaseWm2
        - outgoingSlope * altitudeCoolingK) / outgoingSlope;
}

f32 calculateAnnualMeanInsolation(const ClimateState& climateState, const u32 row) {
    if (!climateState.insolationByTurnRow || climateState.annualTurnCount == 0) {
        return 0.0f;
    }

    f32 accumulatedInsolation = 0.0f;
    for (u32 turnIndex = 0; turnIndex < climateState.annualTurnCount; ++turnIndex) {
        accumulatedInsolation += climateState.insolationByTurnRow[lookupIndex(climateState, turnIndex, row)];
    }
    return accumulatedInsolation / static_cast<f32>(climateState.annualTurnCount);
}

void initializeFromAnnualMeanEquilibrium(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || !climateState.relativeAltitude ||
        !climateState.insolationByTurnRow || climateState.gridWidth == 0 || climateState.gridHeight == 0 ||
        climateState.annualTurnCount == 0) {
        return;
    }

    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const f32 annualMeanInsolation = calculateAnnualMeanInsolation(climateState, row);
        const u32 rowStart = row * climateState.gridWidth;
        for (u32 column = 0; column < climateState.gridWidth; ++column) {
            const u32 index = rowStart + column;
            const f32 surfaceAlbedo = climateState.surfaceAlbedo
                ? climateState.surfaceAlbedo[index]
                : CONFIG.surface.referenceAlbedo;
            const f32 absorbedShortwave = calculateAbsorbedShortwave(annualMeanInsolation, surfaceAlbedo);
            const f32 equilibriumTemperatureC = calculateRadiativeEquilibriumTemperatureC(
                absorbedShortwave,
                calculateAltitudeCoolingK(climateState, index));
            climateState.temperatureKelvin[index] = CONFIG.shared.kelvinOffset + equilibriumTemperatureC;
        }
    }
}

f32 calculateDiffusiveTendencyCPerSecond(
    const f32* previousTemperatureKelvin,
    const ClimateState& climateState,
    const u32 row,
    const u32 column) {
    const u32 centerIndex = flattenIndex(climateState, row, column);
    const f32 centerTemperatureC = sampleTemperatureC(previousTemperatureKelvin, centerIndex);
    const f32 zonalCellWidthMeters = getZonalCellWidthMeters(climateState, row);
    const f32 meridionalCellHeightMeters = getMeridionalCellHeightMeters(climateState);
    const f32 inverseZonalDistanceSquared = 1.0f / std::max(zonalCellWidthMeters * zonalCellWidthMeters, 1.0f);
    const f32 inverseMeridionalDistanceSquared =
        1.0f / std::max(meridionalCellHeightMeters * meridionalCellHeightMeters, 1.0f);

    f32 diffusiveTendencyCPerSecond = 0.0f;

    const auto accumulateTendency = [&](const u32 neighborRow, const u32 neighborColumn, const f32 inverseDistanceSquared) {
        const u32 neighborIndex = flattenIndex(climateState, neighborRow, neighborColumn);
        const f32 neighborTemperatureC = sampleTemperatureC(previousTemperatureKelvin, neighborIndex);
        const f32 thermalDiffusivity = getContactThermalDiffusivity(climateState, centerIndex, neighborIndex);
        diffusiveTendencyCPerSecond +=
            thermalDiffusivity * (neighborTemperatureC - centerTemperatureC) * inverseDistanceSquared;
    };

    accumulateTendency(row, wrapColumn(static_cast<i32>(column) - 1, climateState.gridWidth), inverseZonalDistanceSquared);
    accumulateTendency(row, wrapColumn(static_cast<i32>(column) + 1, climateState.gridWidth), inverseZonalDistanceSquared);

    if (row > 0) {
        accumulateTendency(row - 1, column, inverseMeridionalDistanceSquared);
    }
    if (row + 1 < climateState.gridHeight) {
        accumulateTendency(row + 1, column, inverseMeridionalDistanceSquared);
    }

    return diffusiveTendencyCPerSecond;
}

f32 calculateUpwindGradientEastPerMeter(
    const f32* previousTemperatureKelvin,
    const ClimateState& climateState,
    const u32 row,
    const u32 column,
    const f32 zonalCellWidthMeters,
    const f32 eastVelocityMps) {
    const u32 centerIndex = flattenIndex(climateState, row, column);
    const f32 centerTemperatureC = sampleTemperatureC(previousTemperatureKelvin, centerIndex);
    const u32 westIndex = flattenIndex(
        climateState,
        row,
        wrapColumn(static_cast<i32>(column) - 1, climateState.gridWidth));
    const u32 eastIndex = flattenIndex(
        climateState,
        row,
        wrapColumn(static_cast<i32>(column) + 1, climateState.gridWidth));

    if (eastVelocityMps >= 0.0f) {
        return (centerTemperatureC - sampleTemperatureC(previousTemperatureKelvin, westIndex))
            / std::max(zonalCellWidthMeters, 1.0f);
    }

    return (sampleTemperatureC(previousTemperatureKelvin, eastIndex) - centerTemperatureC)
        / std::max(zonalCellWidthMeters, 1.0f);
}

f32 calculateUpwindGradientSouthPerMeter(
    const f32* previousTemperatureKelvin,
    const ClimateState& climateState,
    const u32 row,
    const u32 column,
    const f32 meridionalCellHeightMeters,
    const f32 southVelocityMps) {
    const u32 centerIndex = flattenIndex(climateState, row, column);
    const f32 centerTemperatureC = sampleTemperatureC(previousTemperatureKelvin, centerIndex);

    if (southVelocityMps >= 0.0f) {
        if (row == 0) {
            return 0.0f;
        }

        const u32 northIndex = flattenIndex(climateState, row - 1, column);
        return (centerTemperatureC - sampleTemperatureC(previousTemperatureKelvin, northIndex))
            / std::max(meridionalCellHeightMeters, 1.0f);
    }

    if (row + 1 >= climateState.gridHeight) {
        return 0.0f;
    }

    const u32 southIndex = flattenIndex(climateState, row + 1, column);
    return (sampleTemperatureC(previousTemperatureKelvin, southIndex) - centerTemperatureC)
        / std::max(meridionalCellHeightMeters, 1.0f);
}

f32 calculateAdvectiveTendencyCPerSecond(
    const f32* previousTemperatureKelvin,
    const ClimateState& climateState,
    const u32 row,
    const u32 column,
    const f32 substepSeconds) {
    if (!climateState.windEastMps || !climateState.windNorthMps) {
        return 0.0f;
    }

    const u32 index = flattenIndex(climateState, row, column);
    const f32 zonalCellWidthMeters = getZonalCellWidthMeters(climateState, row);
    const f32 meridionalCellHeightMeters = getMeridionalCellHeightMeters(climateState);
    const f32 eastVelocityLimit = CONFIG.temperature.maxAdvectiveCourantNumber
        * zonalCellWidthMeters / std::max(substepSeconds, 1.0f);
    const f32 southVelocityLimit = CONFIG.temperature.maxAdvectiveCourantNumber
        * meridionalCellHeightMeters / std::max(substepSeconds, 1.0f);

    const f32 eastVelocityMps = std::clamp(
        climateState.windEastMps[index] * CONFIG.temperature.windAdvectionCoupling,
        -eastVelocityLimit,
        eastVelocityLimit);
    const f32 southVelocityMps = std::clamp(
        -climateState.windNorthMps[index] * CONFIG.temperature.windAdvectionCoupling,
        -southVelocityLimit,
        southVelocityLimit);

    const f32 dTemperatureDx = calculateUpwindGradientEastPerMeter(
        previousTemperatureKelvin,
        climateState,
        row,
        column,
        zonalCellWidthMeters,
        eastVelocityMps);
    const f32 dTemperatureDs = calculateUpwindGradientSouthPerMeter(
        previousTemperatureKelvin,
        climateState,
        row,
        column,
        meridionalCellHeightMeters,
        southVelocityMps);

    return -(eastVelocityMps * dTemperatureDx + southVelocityMps * dTemperatureDs);
}

u32 calculateTransportSubstepCount(const ClimateState& climateState, const f32 timeStepSeconds) {
    const f32 minimumZonalCellWidthMeters =
        getEquatorialCellWidthMeters(climateState) * CONFIG.temperature.minimumZonalCellWidthFactor;
    const f32 meridionalCellHeightMeters = getMeridionalCellHeightMeters(climateState);
    const f32 inverseDistanceScale =
        1.0f / std::max(minimumZonalCellWidthMeters * minimumZonalCellWidthMeters, 1.0f)
        + 1.0f / std::max(meridionalCellHeightMeters * meridionalCellHeightMeters, 1.0f);
    const f32 maximumDiffusivity = std::max(
        CONFIG.temperature.landThermalDiffusivityM2PerS,
        std::max(
            CONFIG.temperature.coastalThermalDiffusivityM2PerS,
            CONFIG.temperature.oceanThermalDiffusivityM2PerS));
    const f32 diffusiveNumber = maximumDiffusivity * timeStepSeconds * inverseDistanceScale;

    f32 maximumWindSpeedMps = 0.0f;
    if (climateState.windEastMps && climateState.windNorthMps) {
        for (u32 index = 0; index < climateState.tileCount; ++index) {
            maximumWindSpeedMps = std::max(maximumWindSpeedMps, std::abs(climateState.windEastMps[index]));
            maximumWindSpeedMps = std::max(maximumWindSpeedMps, std::abs(climateState.windNorthMps[index]));
        }
    }
    const f32 advectiveNumber = CONFIG.temperature.windAdvectionCoupling * maximumWindSpeedMps * timeStepSeconds * (
        1.0f / std::max(minimumZonalCellWidthMeters, 1.0f)
        + 1.0f / std::max(meridionalCellHeightMeters, 1.0f));

    const u32 diffusiveSubsteps = static_cast<u32>(std::max(
        1.0f,
        std::ceil(diffusiveNumber / MAX_DIFFUSIVE_COURANT_NUMBER)));
    const u32 advectiveSubsteps = static_cast<u32>(std::max(
        1.0f,
        std::ceil(advectiveNumber / std::max(CONFIG.temperature.maxAdvectiveCourantNumber, 1e-3f))));

    return std::max(1u, std::max(diffusiveSubsteps, advectiveSubsteps));
}

void advanceEnergyBalanceOneTurn(ClimateState& climateState, const u32 turnIndex) {
    if (!climateState.temperatureKelvin || !climateState.temperatureScratchKelvin ||
        !climateState.relativeAltitude || !climateState.insolationByTurnRow ||
        climateState.gridWidth == 0 || climateState.gridHeight == 0 || climateState.annualTurnCount == 0) {
        return;
    }

    const u32 lookupTurn = turnIndex % climateState.annualTurnCount;
    const f32 timeStepSeconds = static_cast<f32>(
        (ORBITAL_PARAMS.orbitalPeriodDay * 86400.0) / static_cast<f64>(climateState.annualTurnCount));
    const f32 outgoingSlope = std::max(CONFIG.temperature.outgoingLongwaveSlopeWm2PerC, 1e-3f);
    const f32 referenceHeatCapacity = std::max(CONFIG.temperature.referenceHeatCapacityJPerM2K, 1e3f);

    const u32 transportSubstepCount = calculateTransportSubstepCount(climateState, timeStepSeconds);
    const f32 substepSeconds = timeStepSeconds / static_cast<f32>(transportSubstepCount);

    for (u32 substepIndex = 0; substepIndex < transportSubstepCount; ++substepIndex) {
        std::memcpy(
            climateState.temperatureScratchKelvin.get(),
            climateState.temperatureKelvin.get(),
            climateState.tileCount * sizeof(f32));

        const f32* previousTemperatureKelvin = climateState.temperatureScratchKelvin.get();
        for (u32 row = 0; row < climateState.gridHeight; ++row) {
            const f32 turnInsolation = climateState.insolationByTurnRow[lookupIndex(climateState, lookupTurn, row)];
            const u32 rowStart = row * climateState.gridWidth;
            for (u32 column = 0; column < climateState.gridWidth; ++column) {
                const u32 index = rowStart + column;
                const f32 previousTemperatureC = sampleTemperatureC(previousTemperatureKelvin, index);
                const f32 surfaceAlbedo = climateState.surfaceAlbedo
                    ? climateState.surfaceAlbedo[index]
                    : CONFIG.surface.referenceAlbedo;
                const f32 absorbedShortwave = calculateAbsorbedShortwave(turnInsolation, surfaceAlbedo);
                const f32 altitudeCoolingK = calculateAltitudeCoolingK(climateState, index);
                const f32 outgoingLongwave = CONFIG.temperature.outgoingLongwaveBaseWm2
                    + outgoingSlope * (previousTemperatureC + altitudeCoolingK);
                const f32 radiativeTendencyCPerSecond = (absorbedShortwave - outgoingLongwave)
                    / (referenceHeatCapacity * (
                        climateState.effectiveHeatCapacity
                            ? std::max(climateState.effectiveHeatCapacity[index], 1e-3f)
                            : 1.0f));
                const f32 diffusiveTendencyCPerSecond = calculateDiffusiveTendencyCPerSecond(
                    previousTemperatureKelvin,
                    climateState,
                    row,
                    column);
                const f32 advectiveTendencyCPerSecond = calculateAdvectiveTendencyCPerSecond(
                    previousTemperatureKelvin,
                    climateState,
                    row,
                    column,
                    substepSeconds);

                const f32 deltaTemperatureK =
                    (radiativeTendencyCPerSecond + diffusiveTendencyCPerSecond + advectiveTendencyCPerSecond)
                    * substepSeconds;
                climateState.temperatureKelvin[index] = previousTemperatureKelvin[index] + deltaTemperatureK;
            }
        }
    }
}

} // namespace

ClimateState TemperaturePass::createInitialState(const MapResult& mapResult) {
    if (!mapResult.heights || mapResult.width == 0 || mapResult.height == 0) {
        return {};
    }

    const u32 width = mapResult.width;
    const u32 height = mapResult.height;
    const u32 total = width * height;

    ClimateState climateState(width, height);

    f32 maxHeight = mapResult.oceanLevel;
    for (u32 index = 0; index < total; ++index) {
        maxHeight = std::max(maxHeight, mapResult.heights[index]);
    }

    const f32 heightRange = std::max(maxHeight - mapResult.oceanLevel, CONFIG.shared.minHeightRange);

    for (u32 row = 0; row < height; ++row) {
        const f32 latitudeRadians = static_cast<f32>(getLatitudeRadians(row, height));
        for (u32 column = 0; column < width; ++column) {
            const u32 index = row * width + column;
            climateState.latitudeRadians[index] = latitudeRadians;

            const f32 rawHeight = mapResult.heights[index];
            const bool isWater = rawHeight <= mapResult.oceanLevel;
            f32 relativeAltitude = 0.0f;
            if (rawHeight > mapResult.oceanLevel) {
                relativeAltitude = std::clamp((rawHeight - mapResult.oceanLevel) / heightRange, 0.0f, 1.0f);
            }
            climateState.relativeAltitude[index] = relativeAltitude;
            climateState.forestCoverFraction[index] = 0.0f;
            climateState.baseSurfaceAlbedo[index] =
                isWater ? CONFIG.surface.deepWaterAlbedo : CONFIG.surface.landReferenceAlbedo;
            climateState.baseHeatCapacity[index] =
                isWater ? CONFIG.surface.deepWaterHeatCapacity : CONFIG.surface.landHeatCapacity;
            climateState.windEastMps[index] = 0.0f;
            climateState.windNorthMps[index] = 0.0f;
            climateState.snowWaterEquivalent[index] = 0.0f;
            climateState.snowCoverFraction[index] = 0.0f;
            climateState.seaIceFraction[index] = 0.0f;
            climateState.surfaceAlbedo[index] =
                isWater ? CONFIG.surface.deepWaterAlbedo : CONFIG.surface.landReferenceAlbedo;
            climateState.effectiveHeatCapacity[index] =
                isWater ? CONFIG.surface.deepWaterHeatCapacity : CONFIG.surface.landHeatCapacity;
        }
    }

    climateState.annualTurnCount = Astro::DEFAULT_YEAR_TURN_COUNT;
    climateState.insolationByTurnRow =
        std::make_unique<f32[]>(climateState.annualTurnCount * height);
    precomputeInsolationLookup(climateState);

    climateState.absoluteTurnIndex = 0;
    climateState.currentTurnIndex = 0;
    climateState.currentYearFraction = 0.0f;
    initializeCurrentTurn(climateState);

    return climateState;
}

void TemperaturePass::initializeCurrentTurn(ClimateState& climateState) {
    initializeFromAnnualMeanEquilibrium(climateState);
    advanceEnergyBalanceOneTurn(climateState, climateState.currentTurnIndex);
}

void TemperaturePass::advanceOneTurn(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || climateState.tileCount == 0) {
        return;
    }

    const u32 nextTurn = (climateState.currentTurnIndex + 1) % Astro::DEFAULT_YEAR_TURN_COUNT;
    advanceEnergyBalanceOneTurn(climateState, nextTurn);
    ++climateState.absoluteTurnIndex;
    climateState.currentTurnIndex = nextTurn;
    climateState.currentYearFraction =
        static_cast<f32>(nextTurn) / static_cast<f32>(Astro::DEFAULT_YEAR_TURN_COUNT);
}

void TemperaturePass::publishToTiles(const ClimateState& climateState, std::unique_ptr<TileData[]>& tiles) {
    if (!tiles || !climateState.temperatureKelvin) {
        return;
    }

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f32 temperatureCelsius = climateState.temperatureKelvin[index] - CONFIG.shared.kelvinOffset;
        tiles[index].temperature = quantizeTemperature(temperatureCelsius);
    }
}

void TemperaturePass::precomputeInsolationLookup(ClimateState& climateState) {
    if (!climateState.insolationByTurnRow || climateState.gridHeight == 0 ||
        climateState.annualTurnCount == 0) {
        return;
    }

    for (u32 turnIndex = 0; turnIndex < climateState.annualTurnCount; ++turnIndex) {
        const u32 turnStart = turnIndex;
        const u32 turnEnd = turnStart + 1;
        for (u32 row = 0; row < climateState.gridHeight; ++row) {
            const f64 latitudeRadians = getLatitudeRadians(row, climateState.gridHeight);
            const f64 intervalInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
                latitudeRadians,
                turnStart,
                turnEnd,
                STAR_PARAMS,
                ORBITAL_PARAMS,
                climateState.annualTurnCount);

            climateState.insolationByTurnRow[lookupIndex(climateState, turnIndex, row)] =
                static_cast<f32>(intervalInsolation);
        }
    }
}

f32 TemperaturePass::normalizeForOverlay(const i8 temperatureCelsius) {
    const f32 clamped = std::clamp(
        static_cast<f32>(temperatureCelsius),
        CONFIG.temperature.minOverlayTemperatureC,
        CONFIG.temperature.maxOverlayTemperatureC);
    return (clamped - CONFIG.temperature.minOverlayTemperatureC)
        / (CONFIG.temperature.maxOverlayTemperatureC - CONFIG.temperature.minOverlayTemperatureC);
}

f32 TemperaturePass::normalizeKelvinForOverlay(const f32 temperatureKelvin) {
    constexpr f32 minOverlayTemperatureK = CONFIG.shared.kelvinOffset + CONFIG.temperature.minOverlayTemperatureC;
    constexpr f32 maxOverlayTemperatureK =
        CONFIG.shared.kelvinOffset + CONFIG.temperature.maxOverlayTemperatureC;
    const f32 clamped = std::clamp(temperatureKelvin, minOverlayTemperatureK, maxOverlayTemperatureK);
    return (clamped - minOverlayTemperatureK) / (maxOverlayTemperatureK - minOverlayTemperatureK);
}

f64 TemperaturePass::getLatitudeRadians(const u32 row, const u32 height) {
    const f64 normalizedRow = (static_cast<f64>(row) + 0.5) / static_cast<f64>(height);
    return Astro::HALF_PI - normalizedRow * Astro::PI;
}

i8 TemperaturePass::quantizeTemperature(const f32 temperatureCelsius) {
    const f32 clamped = std::clamp(temperatureCelsius, -128.0f, 127.0f);
    return static_cast<i8>(std::lround(clamped));
}