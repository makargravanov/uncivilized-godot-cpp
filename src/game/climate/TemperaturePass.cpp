//
// Created by Copilot on 06.03.2026.
//

#include "TemperaturePass.h"

#include <algorithm>
#include <cmath>

#include "Astrophysics.h"
#include "ClimateConfig.h"
#include "game/map/elevations-creation/PlatecWrapper.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

u32 lookupIndex(const ClimateState& climateState, const u32 turnIndex, const u32 row) {
    return turnIndex * climateState.gridHeight + row;
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
            climateState.surfaceAlbedo[index] =
                isWater ? CONFIG.surface.deepWaterAlbedo : CONFIG.surface.landReferenceAlbedo;
            climateState.effectiveHeatCapacity[index] =
                isWater ? CONFIG.surface.deepWaterHeatCapacity : CONFIG.surface.landHeatCapacity;
        }
    }

    climateState.seaLevelTemperatureTurnCount = Astro::DEFAULT_YEAR_TURN_COUNT;
    climateState.seaLevelTemperatureByTurnRow =
        std::make_unique<f32[]>(climateState.seaLevelTemperatureTurnCount * height);
    precomputeSeaLevelTemperatureLookup(climateState);

    climateState.absoluteTurnIndex = 0;
    climateState.currentTurnIndex = 0;
    climateState.currentYearFraction = 0.0f;
    initializeCurrentTurn(climateState);

    return climateState;
}

void TemperaturePass::initializeCurrentTurn(ClimateState& climateState) {
    blendTowardTurnTarget(climateState, climateState.currentTurnIndex, 1.0f);
}

void TemperaturePass::advanceOneTurn(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || climateState.tileCount == 0) {
        return;
    }

    const u32 nextTurn = (climateState.currentTurnIndex + 1) % Astro::DEFAULT_YEAR_TURN_COUNT;
    blendTowardTurnTarget(climateState, nextTurn, CONFIG.temperature.turnRelaxationFactor);
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

void TemperaturePass::precomputeSeaLevelTemperatureLookup(ClimateState& climateState) {
    if (!climateState.seaLevelTemperatureByTurnRow || climateState.gridHeight == 0 ||
        climateState.seaLevelTemperatureTurnCount == 0) {
        return;
    }

    const Astro::StarParams starParams;
    const Astro::OrbitalParams orbitalParams;
    constexpr f32 polarSeaLevelTemperatureK = CONFIG.shared.kelvinOffset + CONFIG.temperature.polarSeaLevelTemperatureC;
    constexpr f32 equatorSeaLevelTemperatureK =
        CONFIG.shared.kelvinOffset + CONFIG.temperature.equatorSeaLevelTemperatureC;

    for (u32 turnIndex = 0; turnIndex < climateState.seaLevelTemperatureTurnCount; ++turnIndex) {
        const u32 turnStart = turnIndex;
        const u32 turnEnd = turnStart + 1;

        f64 minInsolation = 1e18;
        f64 maxInsolation = -1e18;
        for (u32 sampleIndex = 0; sampleIndex < CONFIG.temperature.latitudeSampleCount; ++sampleIndex) {
            const f64 sampleLatitude = -Astro::HALF_PI + sampleIndex * (
                Astro::PI / (CONFIG.temperature.latitudeSampleCount - 1));
            const f64 sampleInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
                sampleLatitude, turnStart, turnEnd, starParams, orbitalParams);
            minInsolation = std::min(minInsolation, sampleInsolation);
            maxInsolation = std::max(maxInsolation, sampleInsolation);
        }

        const f64 insolationRange = std::max(
            maxInsolation - minInsolation,
            static_cast<f64>(CONFIG.shared.minHeightRange));
        for (u32 row = 0; row < climateState.gridHeight; ++row) {
            const f64 latitudeRadians = getLatitudeRadians(row, climateState.gridHeight);
            const f64 intervalInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
                latitudeRadians,
                turnStart,
                turnEnd,
                starParams,
                orbitalParams);
            const f32 normalizedInsolation = static_cast<f32>(std::clamp(
                (intervalInsolation - minInsolation) / insolationRange,
                0.0,
                1.0));

            climateState.seaLevelTemperatureByTurnRow[lookupIndex(climateState, turnIndex, row)] =
                polarSeaLevelTemperatureK
                + normalizedInsolation * (equatorSeaLevelTemperatureK - polarSeaLevelTemperatureK);
        }
    }
}

void TemperaturePass::blendTowardTurnTarget(ClimateState& climateState, const u32 turnIndex, const f32 blendFactor) {
    if (!climateState.temperatureKelvin || !climateState.relativeAltitude ||
        !climateState.seaLevelTemperatureByTurnRow || climateState.gridWidth == 0 || climateState.gridHeight == 0 ||
        climateState.seaLevelTemperatureTurnCount == 0) {
        return;
    }

    const u32 lookupTurn = turnIndex % climateState.seaLevelTemperatureTurnCount;

    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        const f32 seaLevelTargetK = climateState.seaLevelTemperatureByTurnRow[
            lookupIndex(climateState, lookupTurn, row)];
        const u32 rowStart = row * climateState.gridWidth;
        for (u32 column = 0; column < climateState.gridWidth; ++column) {
            const u32 index = rowStart + column;
            const f32 altitudeCoolingK =
                climateState.relativeAltitude[index] * CONFIG.temperature.maxAltitudeCoolingK;
            const f32 surfaceAlbedo = climateState.surfaceAlbedo
                ? climateState.surfaceAlbedo[index]
                : CONFIG.surface.referenceAlbedo;
            const f32 albedoCoolingK =
                (surfaceAlbedo - CONFIG.surface.referenceAlbedo) * CONFIG.surface.albedoTemperatureResponseK;
            const f32 targetTemperatureK = seaLevelTargetK - altitudeCoolingK - albedoCoolingK;

            if (blendFactor >= 1.0f) {
                climateState.temperatureKelvin[index] = targetTemperatureK;
                continue;
            }

            const f32 effectiveHeatCapacity = climateState.effectiveHeatCapacity
                ? std::max(climateState.effectiveHeatCapacity[index], 1e-3f)
                : CONFIG.surface.landHeatCapacity;
            const f32 localBlendFactor = std::clamp(
                blendFactor / effectiveHeatCapacity,
                CONFIG.surface.minTurnResponseFactor,
                CONFIG.surface.maxTurnResponseFactor);

            climateState.temperatureKelvin[index] +=
                (targetTemperatureK - climateState.temperatureKelvin[index]) * localBlendFactor;
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