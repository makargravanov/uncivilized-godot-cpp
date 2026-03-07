//
// Created by Copilot on 06.03.2026.
//

#include "TemperaturePass.h"

#include <algorithm>
#include <cmath>

#include "Astrophysics.h"
#include "game/map/elevations-creation/PlatecWrapper.h"

namespace {

constexpr f32 KELVIN_OFFSET = 273.15f;
constexpr f32 MIN_OVERLAY_TEMPERATURE_C = -40.0f;
constexpr f32 MAX_OVERLAY_TEMPERATURE_C = 40.0f;
constexpr f32 MIN_OVERLAY_TEMPERATURE_K = KELVIN_OFFSET + MIN_OVERLAY_TEMPERATURE_C;
constexpr f32 MAX_OVERLAY_TEMPERATURE_K = KELVIN_OFFSET + MAX_OVERLAY_TEMPERATURE_C;
constexpr f32 POLAR_SEA_LEVEL_TEMPERATURE_K = KELVIN_OFFSET - 22.0f;
constexpr f32 EQUATOR_SEA_LEVEL_TEMPERATURE_K = KELVIN_OFFSET + 32.0f;
constexpr f32 MAX_ALTITUDE_COOLING_K = 38.0f;
constexpr f32 TURN_RELAXATION_FACTOR = 0.35f;
constexpr f32 MIN_HEIGHT_RANGE = 1e-6f;

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

    const f32 heightRange = std::max(maxHeight - mapResult.oceanLevel, MIN_HEIGHT_RANGE);

    for (u32 row = 0; row < height; ++row) {
        const f32 latitudeRadians = static_cast<f32>(getLatitudeRadians(row, height));
        for (u32 column = 0; column < width; ++column) {
            const u32 index = row * width + column;
            climateState.latitudeRadians[index] = latitudeRadians;

            const f32 rawHeight = mapResult.heights[index];
            f32 relativeAltitude = 0.0f;
            if (rawHeight > mapResult.oceanLevel) {
                relativeAltitude = std::clamp((rawHeight - mapResult.oceanLevel) / heightRange, 0.0f, 1.0f);
            }
            climateState.relativeAltitude[index] = relativeAltitude;
        }
    }

    climateState.absoluteTurnIndex = 0;
    climateState.currentTurnIndex = 0;
    climateState.currentYearFraction = 0.0f;
    blendTowardTurnTarget(climateState, 0, 1.0f);

    return climateState;
}

void TemperaturePass::advanceOneTurn(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || climateState.tileCount == 0) {
        return;
    }

    const u32 nextTurn = (climateState.currentTurnIndex + 1) % Astro::DEFAULT_YEAR_TURN_COUNT;
    blendTowardTurnTarget(climateState, nextTurn, TURN_RELAXATION_FACTOR);
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
        const f32 temperatureCelsius = climateState.temperatureKelvin[index] - KELVIN_OFFSET;
        tiles[index].temperature = quantizeTemperature(temperatureCelsius);
    }
}

void TemperaturePass::blendTowardTurnTarget(ClimateState& climateState, const u32 turnIndex, const f32 blendFactor) {
    const Astro::StarParams starParams;
    const Astro::OrbitalParams orbitalParams;

    const u32 turnStart = turnIndex % Astro::DEFAULT_YEAR_TURN_COUNT;
    const u32 turnEnd = turnStart + 1;

    // Sample insolation at several latitudes to find the actual min/max for this turn.
    // During polar summer the pole can receive MORE insolation than the equator,
    // so we cannot assume pole=min, equator=max.
    constexpr u32 LATITUDE_SAMPLE_COUNT = 19;  // every 10° from -90 to +90
    f64 minInsolation = 1e18;
    f64 maxInsolation = -1e18;
    for (u32 s = 0; s < LATITUDE_SAMPLE_COUNT; ++s) {
        const f64 sampleLatitude = -Astro::HALF_PI + s * (Astro::PI / (LATITUDE_SAMPLE_COUNT - 1));
        const f64 sampleInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
            sampleLatitude, turnStart, turnEnd, starParams, orbitalParams);
        minInsolation = std::min(minInsolation, sampleInsolation);
        maxInsolation = std::max(maxInsolation, sampleInsolation);
    }
    const f64 insolationRange = std::max(maxInsolation - minInsolation, static_cast<f64>(MIN_HEIGHT_RANGE));

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f64 intervalInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
            climateState.latitudeRadians[index],
            turnStart,
            turnEnd,
            starParams,
            orbitalParams);
        const f32 normalizedInsolation = static_cast<f32>(std::clamp(
            (intervalInsolation - minInsolation) / insolationRange,
            0.0,
            1.0));

        const f32 seaLevelTargetK = POLAR_SEA_LEVEL_TEMPERATURE_K
            + normalizedInsolation * (EQUATOR_SEA_LEVEL_TEMPERATURE_K - POLAR_SEA_LEVEL_TEMPERATURE_K);
        const f32 altitudeCoolingK = climateState.relativeAltitude[index] * MAX_ALTITUDE_COOLING_K;
        const f32 targetTemperatureK = seaLevelTargetK - altitudeCoolingK;

        climateState.temperatureKelvin[index] +=
            (targetTemperatureK - climateState.temperatureKelvin[index]) * blendFactor;
    }
}

f32 TemperaturePass::normalizeForOverlay(const i8 temperatureCelsius) {
    const f32 clamped = std::clamp(static_cast<f32>(temperatureCelsius), MIN_OVERLAY_TEMPERATURE_C, MAX_OVERLAY_TEMPERATURE_C);
    return (clamped - MIN_OVERLAY_TEMPERATURE_C) / (MAX_OVERLAY_TEMPERATURE_C - MIN_OVERLAY_TEMPERATURE_C);
}

f32 TemperaturePass::normalizeKelvinForOverlay(const f32 temperatureKelvin) {
    const f32 clamped = std::clamp(temperatureKelvin, MIN_OVERLAY_TEMPERATURE_K, MAX_OVERLAY_TEMPERATURE_K);
    return (clamped - MIN_OVERLAY_TEMPERATURE_K) / (MAX_OVERLAY_TEMPERATURE_K - MIN_OVERLAY_TEMPERATURE_K);
}

f64 TemperaturePass::getLatitudeRadians(const u32 row, const u32 height) {
    const f64 normalizedRow = (static_cast<f64>(row) + 0.5) / static_cast<f64>(height);
    return Astro::HALF_PI - normalizedRow * Astro::PI;
}

i8 TemperaturePass::quantizeTemperature(const f32 temperatureCelsius) {
    const f32 clamped = std::clamp(temperatureCelsius, -128.0f, 127.0f);
    return static_cast<i8>(std::lround(clamped));
}