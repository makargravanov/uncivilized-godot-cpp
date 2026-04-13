//
// Created by Copilot on 06.03.2026.
//

#include "TemperaturePass.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <godot_cpp/core/error_macros.hpp>

#include "Astrophysics.h"
#include "ClimateConfig.h"
#include "game/map/elevations-creation/PlatecWrapper.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;
constexpr Astro::StarParams STAR_PARAMS;
constexpr Astro::OrbitalParams ORBITAL_PARAMS;

struct LinearDiffusionCoefficients {
    f32 equilibriumSourceCPerSecond = 0.0f;
    f32 relaxationRatePerSecond = 0.0f;
};

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

#if defined(DEBUG_ENABLED)
void failIfNonFinite(const f32 value, const char* message) {
    CRASH_COND_MSG(!std::isfinite(value), message);
}
#else
inline void failIfNonFinite(const f32, const char*) {}
#endif

f32 getPlanetRadiusMeters() {
    return std::max(CONFIG.temperature.planetRadiusMeters, 1.0e3f);
}

f32 getEquatorialCellWidthMeters(const ClimateState& climateState) {
    return static_cast<f32>((Astro::TWO_PI * getPlanetRadiusMeters()) / static_cast<f64>(climateState.gridWidth));
}

f32 getMeridionalCellHeightMeters(const ClimateState& climateState) {
    if (climateState.meridionalCellHeightMeters > 0.0f) {
        return climateState.meridionalCellHeightMeters;
    }

    return static_cast<f32>((Astro::PI * getPlanetRadiusMeters()) / static_cast<f64>(climateState.gridHeight));
}

f32 getZonalCellWidthMeters(const ClimateState& climateState, const u32 row) {
    if (climateState.zonalCellWidthMetersByRow && row < climateState.gridHeight) {
        return climateState.zonalCellWidthMetersByRow[row];
    }

    const u32 rowIndex = flattenIndex(climateState, row, 0);
    const f32 latitudeRadians = climateState.latitudeRadians
        ? climateState.latitudeRadians[rowIndex]
        : 0.0f;
    const f32 zonalWidthFactor = std::max(
        std::cos(latitudeRadians),
        CONFIG.temperature.minimumZonalCellWidthFactor);
    return getEquatorialCellWidthMeters(climateState) * zonalWidthFactor;
}

f32 getInverseZonalCellWidth(const ClimateState& climateState, const u32 row) {
    if (climateState.inverseZonalCellWidthByRow && row < climateState.gridHeight) {
        return climateState.inverseZonalCellWidthByRow[row];
    }

    return 1.0f / std::max(getZonalCellWidthMeters(climateState, row), 1.0f);
}

f32 getInverseZonalDistanceSquared(const ClimateState& climateState, const u32 row) {
    if (climateState.inverseZonalDistanceSquaredByRow && row < climateState.gridHeight) {
        return climateState.inverseZonalDistanceSquaredByRow[row];
    }

    const f32 zonalCellWidthMeters = getZonalCellWidthMeters(climateState, row);
    return 1.0f / std::max(zonalCellWidthMeters * zonalCellWidthMeters, 1.0f);
}

f32 getInverseMeridionalCellHeight(const ClimateState& climateState) {
    if (climateState.inverseMeridionalCellHeightMeters > 0.0f) {
        return climateState.inverseMeridionalCellHeightMeters;
    }

    return 1.0f / std::max(getMeridionalCellHeightMeters(climateState), 1.0f);
}

f32 getInverseMeridionalDistanceSquared(const ClimateState& climateState) {
    if (climateState.inverseMeridionalDistanceSquared > 0.0f) {
        return climateState.inverseMeridionalDistanceSquared;
    }

    const f32 meridionalCellHeightMeters = getMeridionalCellHeightMeters(climateState);
    return 1.0f / std::max(meridionalCellHeightMeters * meridionalCellHeightMeters, 1.0f);
}

void precomputeTransportGeometry(ClimateState& climateState) {
    climateState.meridionalCellHeightMeters = static_cast<f32>(
        (Astro::PI * getPlanetRadiusMeters()) / static_cast<f64>(std::max(climateState.gridHeight, 1u)));
    climateState.inverseMeridionalCellHeightMeters =
        1.0f / std::max(climateState.meridionalCellHeightMeters, 1.0f);
    climateState.inverseMeridionalDistanceSquared =
        climateState.inverseMeridionalCellHeightMeters * climateState.inverseMeridionalCellHeightMeters;

    if (!climateState.zonalCellWidthMetersByRow || !climateState.inverseZonalCellWidthByRow ||
        !climateState.inverseZonalDistanceSquaredByRow) {
        return;
    }

    const f32 equatorialCellWidthMeters = static_cast<f32>(
        (Astro::TWO_PI * getPlanetRadiusMeters()) / static_cast<f64>(std::max(climateState.gridWidth, 1u)));
    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const u32 rowIndex = flattenIndex(climateState, row, 0);
        const f32 latitudeRadians = climateState.latitudeRadians ? climateState.latitudeRadians[rowIndex] : 0.0f;
        const f32 zonalWidthFactor = std::max(
            std::cos(latitudeRadians),
            CONFIG.temperature.minimumZonalCellWidthFactor);
        const f32 zonalCellWidthMeters = equatorialCellWidthMeters * zonalWidthFactor;
        const f32 inverseZonalCellWidth = 1.0f / std::max(zonalCellWidthMeters, 1.0f);

        climateState.zonalCellWidthMetersByRow[row] = zonalCellWidthMeters;
        climateState.inverseZonalCellWidthByRow[row] = inverseZonalCellWidth;
        climateState.inverseZonalDistanceSquaredByRow[row] = inverseZonalCellWidth * inverseZonalCellWidth;
    }
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

f32 sampleTransportTemperatureC(const f32* temperatureKelvin, const ClimateState& climateState, const u32 index) {
    return sampleTemperatureC(temperatureKelvin, index) + calculateAltitudeCoolingK(climateState, index);
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

LinearDiffusionCoefficients calculateDiffusionLinearCoefficients(
    const f32* previousTemperatureKelvin,
    const ClimateState& climateState,
    const u32 row,
    const u32 column) {
    const u32 centerIndex = flattenIndex(climateState, row, column);
    const f32 inverseZonalDistanceSquared = getInverseZonalDistanceSquared(climateState, row);
    const f32 inverseMeridionalDistanceSquared = getInverseMeridionalDistanceSquared(climateState);
    LinearDiffusionCoefficients coefficients;

    const auto accumulateTendency = [&](const u32 neighborRow, const u32 neighborColumn, const f32 inverseDistanceSquared) {
        const u32 neighborIndex = flattenIndex(climateState, neighborRow, neighborColumn);
        const f32 neighborTemperatureC = sampleTransportTemperatureC(
            previousTemperatureKelvin,
            climateState,
            neighborIndex);
        const f32 thermalDiffusivity = getContactThermalDiffusivity(climateState, centerIndex, neighborIndex);
        coefficients.equilibriumSourceCPerSecond +=
            thermalDiffusivity * neighborTemperatureC * inverseDistanceSquared;
        coefficients.relaxationRatePerSecond += thermalDiffusivity * inverseDistanceSquared;
    };

    accumulateTendency(row, wrapColumn(static_cast<i32>(column) - 1, climateState.gridWidth), inverseZonalDistanceSquared);
    accumulateTendency(row, wrapColumn(static_cast<i32>(column) + 1, climateState.gridWidth), inverseZonalDistanceSquared);

    if (row > 0) {
        accumulateTendency(row - 1, column, inverseMeridionalDistanceSquared);
    }
    if (row + 1 < climateState.gridHeight) {
        accumulateTendency(row + 1, column, inverseMeridionalDistanceSquared);
    }

    failIfNonFinite(coefficients.equilibriumSourceCPerSecond,
        "Non-finite diffusion source in TemperaturePass::calculateDiffusionLinearCoefficients().");
    failIfNonFinite(coefficients.relaxationRatePerSecond,
        "Non-finite diffusion relaxation rate in TemperaturePass::calculateDiffusionLinearCoefficients().");

    return coefficients;
}

f32 calculateUpwindGradientEastPerMeter(
    const f32* previousTemperatureKelvin,
    const ClimateState& climateState,
    const u32 row,
    const u32 column,
    const f32 zonalCellWidthMeters,
    const f32 eastVelocityMps) {
    const u32 centerIndex = flattenIndex(climateState, row, column);
    const f32 centerTemperatureC = sampleTransportTemperatureC(previousTemperatureKelvin, climateState, centerIndex);
    const u32 westIndex = flattenIndex(
        climateState,
        row,
        wrapColumn(static_cast<i32>(column) - 1, climateState.gridWidth));
    const u32 eastIndex = flattenIndex(
        climateState,
        row,
        wrapColumn(static_cast<i32>(column) + 1, climateState.gridWidth));

    if (eastVelocityMps >= 0.0f) {
        return (centerTemperatureC - sampleTransportTemperatureC(previousTemperatureKelvin, climateState, westIndex))
            / std::max(zonalCellWidthMeters, 1.0f);
    }

    return (sampleTransportTemperatureC(previousTemperatureKelvin, climateState, eastIndex) - centerTemperatureC)
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
    const f32 centerTemperatureC = sampleTransportTemperatureC(previousTemperatureKelvin, climateState, centerIndex);

    if (southVelocityMps >= 0.0f) {
        if (row == 0) {
            return 0.0f;
        }

        const u32 northIndex = flattenIndex(climateState, row - 1, column);
        return (centerTemperatureC - sampleTransportTemperatureC(previousTemperatureKelvin, climateState, northIndex))
            / std::max(meridionalCellHeightMeters, 1.0f);
    }

    if (row + 1 >= climateState.gridHeight) {
        return 0.0f;
    }

    const u32 southIndex = flattenIndex(climateState, row + 1, column);
    return (sampleTransportTemperatureC(previousTemperatureKelvin, climateState, southIndex) - centerTemperatureC)
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

    std::memcpy(
        climateState.temperatureScratchKelvin.get(),
        climateState.temperatureKelvin.get(),
        climateState.tileCount * sizeof(f32));

    const f32* previousTemperatureKelvin = climateState.temperatureScratchKelvin.get();
    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const f32 turnInsolation = climateState.insolationByTurnRow[lookupIndex(climateState, lookupTurn, row)];
        failIfNonFinite(turnInsolation,
            "Non-finite insolation row value in TemperaturePass::advanceEnergyBalanceOneTurn().");
        const u32 rowStart = row * climateState.gridWidth;
        for (u32 column = 0; column < climateState.gridWidth; ++column) {
            const u32 index = rowStart + column;
            const f32 previousTemperatureC = sampleTemperatureC(previousTemperatureKelvin, index);
            failIfNonFinite(previousTemperatureC,
                "Non-finite previous temperature in TemperaturePass::advanceEnergyBalanceOneTurn().");
            const f32 altitudeCoolingK = calculateAltitudeCoolingK(climateState, index);
            failIfNonFinite(altitudeCoolingK,
                "Non-finite altitude cooling in TemperaturePass::advanceEnergyBalanceOneTurn().");
            const f32 previousTransportTemperatureC = previousTemperatureC + altitudeCoolingK;
            failIfNonFinite(previousTransportTemperatureC,
                "Non-finite previous transport temperature in TemperaturePass::advanceEnergyBalanceOneTurn().");
            const f32 surfaceAlbedo = climateState.surfaceAlbedo
                ? climateState.surfaceAlbedo[index]
                : CONFIG.surface.referenceAlbedo;
            failIfNonFinite(surfaceAlbedo,
                "Non-finite surface albedo in TemperaturePass::advanceEnergyBalanceOneTurn().");
            const f32 absorbedShortwave = calculateAbsorbedShortwave(turnInsolation, surfaceAlbedo);
            failIfNonFinite(absorbedShortwave,
                "Non-finite absorbed shortwave in TemperaturePass::advanceEnergyBalanceOneTurn().");
            const f32 effectiveHeatCapacity = climateState.effectiveHeatCapacity
                ? std::max(climateState.effectiveHeatCapacity[index], 1e-3f)
                : 1.0f;
            failIfNonFinite(effectiveHeatCapacity,
                "Non-finite effective heat capacity in TemperaturePass::advanceEnergyBalanceOneTurn().");
            const f32 arealHeatCapacity = referenceHeatCapacity * effectiveHeatCapacity;
            failIfNonFinite(arealHeatCapacity,
                "Non-finite areal heat capacity in TemperaturePass::advanceEnergyBalanceOneTurn().");

            const f32 radiativeSourceCPerSecond =
                (absorbedShortwave
                    - CONFIG.temperature.outgoingLongwaveBaseWm2) / arealHeatCapacity;
            const f32 radiativeRelaxationRatePerSecond = outgoingSlope / arealHeatCapacity;
            failIfNonFinite(radiativeSourceCPerSecond,
                "Non-finite radiative source in TemperaturePass::advanceEnergyBalanceOneTurn().");
            failIfNonFinite(radiativeRelaxationRatePerSecond,
                "Non-finite radiative relaxation rate in TemperaturePass::advanceEnergyBalanceOneTurn().");
            const LinearDiffusionCoefficients diffusionCoefficients = calculateDiffusionLinearCoefficients(
                previousTemperatureKelvin,
                climateState,
                row,
                column);
            const f32 totalRelaxationRatePerSecond =
                radiativeRelaxationRatePerSecond + diffusionCoefficients.relaxationRatePerSecond;
            const f32 totalSourceCPerSecond =
                radiativeSourceCPerSecond + diffusionCoefficients.equilibriumSourceCPerSecond;
            failIfNonFinite(totalRelaxationRatePerSecond,
                "Non-finite total relaxation rate in TemperaturePass::advanceEnergyBalanceOneTurn().");
            failIfNonFinite(totalSourceCPerSecond,
                "Non-finite total source in TemperaturePass::advanceEnergyBalanceOneTurn().");

            f32 transportTemperatureAfterRadiativeDiffusiveStepC = previousTransportTemperatureC;
            if (totalRelaxationRatePerSecond > 1e-9f) {
                const f32 equilibriumTemperatureC = totalSourceCPerSecond / totalRelaxationRatePerSecond;
                const f32 decay = std::exp(-totalRelaxationRatePerSecond * timeStepSeconds);
                failIfNonFinite(equilibriumTemperatureC,
                    "Non-finite equilibrium temperature in TemperaturePass::advanceEnergyBalanceOneTurn().");
                failIfNonFinite(decay,
                    "Non-finite decay factor in TemperaturePass::advanceEnergyBalanceOneTurn().");
                transportTemperatureAfterRadiativeDiffusiveStepC =
                    equilibriumTemperatureC + (previousTransportTemperatureC - equilibriumTemperatureC) * decay;
            } else {
                transportTemperatureAfterRadiativeDiffusiveStepC += totalSourceCPerSecond * timeStepSeconds;
            }
            failIfNonFinite(transportTemperatureAfterRadiativeDiffusiveStepC,
                "Non-finite radiative-diffusive transport temperature in TemperaturePass::advanceEnergyBalanceOneTurn().");

            const f32 updatedTemperatureKelvin = CONFIG.shared.kelvinOffset
                + (transportTemperatureAfterRadiativeDiffusiveStepC - altitudeCoolingK);
            failIfNonFinite(updatedTemperatureKelvin,
                "Non-finite updated temperature in TemperaturePass::advanceEnergyBalanceOneTurn().");
            climateState.temperatureKelvin[index] = updatedTemperatureKelvin;
        }
    }

    std::memcpy(
        climateState.temperatureScratchKelvin.get(),
        climateState.temperatureKelvin.get(),
        climateState.tileCount * sizeof(f32));

    previousTemperatureKelvin = climateState.temperatureScratchKelvin.get();
    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const u32 rowStart = row * climateState.gridWidth;
        for (u32 column = 0; column < climateState.gridWidth; ++column) {
            const u32 index = rowStart + column;
            const f32 altitudeCoolingK = calculateAltitudeCoolingK(climateState, index);
            const f32 previousTransportTemperatureC = sampleTransportTemperatureC(
                previousTemperatureKelvin,
                climateState,
                index);
            const f32 advectiveTendencyCPerSecond = calculateAdvectiveTendencyCPerSecond(
                previousTemperatureKelvin,
                climateState,
                row,
                column,
                timeStepSeconds);
            failIfNonFinite(advectiveTendencyCPerSecond,
                "Non-finite advective tendency in TemperaturePass::advanceEnergyBalanceOneTurn().");

            const f32 advectedTransportTemperatureC = previousTransportTemperatureC
                + advectiveTendencyCPerSecond * timeStepSeconds;
            const f32 advectedTemperatureKelvin = CONFIG.shared.kelvinOffset
                + (advectedTransportTemperatureC - altitudeCoolingK);
            failIfNonFinite(advectedTemperatureKelvin,
                "Non-finite advected temperature in TemperaturePass::advanceEnergyBalanceOneTurn().");
            climateState.temperatureKelvin[index] = advectedTemperatureKelvin;
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
    precomputeTransportGeometry(climateState);
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