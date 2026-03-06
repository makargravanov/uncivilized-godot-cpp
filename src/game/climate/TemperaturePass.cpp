//
// Created by Copilot on 06.03.2026.
//

#include "TemperaturePass.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Astrophysics.h"
#include "game/map/elevations-creation/PlatecWrapper.h"

namespace {

constexpr f32 MinOverlayTemperatureC = -40.0f;
constexpr f32 MaxOverlayTemperatureC = 40.0f;
constexpr f32 PolarSeaLevelTemperatureC = -22.0f;
constexpr f32 EquatorSeaLevelTemperatureC = 32.0f;
constexpr f32 MaxAltitudeCoolingC = 38.0f;
constexpr f64 PoleLatitudeOffset = 1e-4;
constexpr f32 MinHeightRange = 1e-6f;

} // namespace

void TemperaturePass::apply(std::unique_ptr<TileData[]>& tiles, const MapResult& mapResult) {
    if (!tiles || !mapResult.heights || mapResult.width == 0 || mapResult.height == 0) {
        return;
    }

    const u32 width = mapResult.width;
    const u32 height = mapResult.height;
    const u32 total = width * height;

    f32 maxHeight = mapResult.oceanLevel;
    for (u32 index = 0; index < total; ++index) {
        maxHeight = std::max(maxHeight, mapResult.heights[index]);
    }

    const f32 heightRange = std::max(maxHeight - mapResult.oceanLevel, MinHeightRange);

    const Astro::StarParams starParams;
    const Astro::OrbitalParams orbitalParams;
    const f64 poleInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
        Astro::HALF_PI - PoleLatitudeOffset,
        0,
        Astro::DEFAULT_YEAR_TURN_COUNT,
        starParams,
        orbitalParams);
    const f64 equatorInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
        0.0,
        0,
        Astro::DEFAULT_YEAR_TURN_COUNT,
        starParams,
        orbitalParams);
    const f64 insolationRange = std::max(equatorInsolation - poleInsolation, static_cast<f64>(MinHeightRange));

    std::vector<f32> rowBaseTemperature(height, 0.0f);

    // Latitude depends only on the row in the current rectangular map projection.
    for (u32 row = 0; row < height; ++row) {
        const f64 latitudeRadians = getLatitudeRadians(row, height);
        const f64 annualInsolation = Astro::Astrophysics::calculateAverageInsolationForTurnRange(
            latitudeRadians,
            0,
            Astro::DEFAULT_YEAR_TURN_COUNT,
            starParams,
            orbitalParams);
        const f32 normalizedInsolation = static_cast<f32>(std::clamp(
            (annualInsolation - poleInsolation) / insolationRange,
            0.0,
            1.0));

        rowBaseTemperature[row] = PolarSeaLevelTemperatureC
            + normalizedInsolation * (EquatorSeaLevelTemperatureC - PolarSeaLevelTemperatureC);
    }

    for (u32 row = 0; row < height; ++row) {
        for (u32 column = 0; column < width; ++column) {
            const u32 index = row * width + column;
            const f32 rawHeight = mapResult.heights[index];

            f32 heightFactor = 0.0f;
            if (rawHeight > mapResult.oceanLevel) {
                heightFactor = std::clamp((rawHeight - mapResult.oceanLevel) / heightRange, 0.0f, 1.0f);
            }

            const f32 altitudeCooling = heightFactor * MaxAltitudeCoolingC;
            const f32 temperatureCelsius = rowBaseTemperature[row] - altitudeCooling;
            tiles[index].temperature = quantizeTemperature(temperatureCelsius);
        }
    }
}

f32 TemperaturePass::normalizeForOverlay(const i8 temperatureCelsius) {
    const f32 clamped = std::clamp(static_cast<f32>(temperatureCelsius), MinOverlayTemperatureC, MaxOverlayTemperatureC);
    return (clamped - MinOverlayTemperatureC) / (MaxOverlayTemperatureC - MinOverlayTemperatureC);
}

f64 TemperaturePass::getLatitudeRadians(const u32 row, const u32 height) {
    const f64 normalizedRow = (static_cast<f64>(row) + 0.5) / static_cast<f64>(height);
    return Astro::HALF_PI - normalizedRow * Astro::PI;
}

i8 TemperaturePass::quantizeTemperature(const f32 temperatureCelsius) {
    const f32 clamped = std::clamp(temperatureCelsius, -128.0f, 127.0f);
    return static_cast<i8>(std::lround(clamped));
}