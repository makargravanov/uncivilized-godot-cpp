//
// Created by Copilot on 06.03.2026.
//

#include "WindPass.h"

#include <algorithm>
#include <cmath>

#include "Astrophysics.h"

namespace {

constexpr f32 MAX_SEASONAL_SHIFT_RADIANS = static_cast<f32>(8.0 * Astro::DEG_TO_RAD);
constexpr f32 CALM_EQUATOR_RADIANS = static_cast<f32>(6.0 * Astro::DEG_TO_RAD);
constexpr f32 BAND_FADE_RADIANS = static_cast<f32>(7.0 * Astro::DEG_TO_RAD);
constexpr f32 TRADE_BAND_START_RADIANS = static_cast<f32>(4.0 * Astro::DEG_TO_RAD);
constexpr f32 TRADE_BAND_END_RADIANS = static_cast<f32>(30.0 * Astro::DEG_TO_RAD);
constexpr f32 WESTERLY_BAND_START_RADIANS = static_cast<f32>(24.0 * Astro::DEG_TO_RAD);
constexpr f32 WESTERLY_BAND_END_RADIANS = static_cast<f32>(60.0 * Astro::DEG_TO_RAD);
constexpr f32 POLAR_BAND_START_RADIANS = static_cast<f32>(56.0 * Astro::DEG_TO_RAD);
constexpr f32 POLAR_BAND_END_RADIANS = static_cast<f32>(86.0 * Astro::DEG_TO_RAD);

constexpr f32 TRADE_WIND_EAST_MPS = -8.0f;
constexpr f32 WESTERLY_WIND_EAST_MPS = 11.0f;
constexpr f32 POLAR_EASTERLY_WIND_EAST_MPS = -5.0f;

constexpr f32 TRADE_WIND_MERIDIONAL_MPS = 3.5f;
constexpr f32 WESTERLY_WIND_MERIDIONAL_MPS = 2.75f;
constexpr f32 POLAR_EASTERLY_WIND_MERIDIONAL_MPS = 1.75f;

constexpr f32 TILE_STEP_X = 173.205078f / 100.0f;
constexpr f32 TILE_STEP_Z = 150.0f / 100.0f;
constexpr f32 OROGRAPHIC_DRAG_FACTOR = 2.8f;
constexpr f32 MAX_OROGRAPHIC_DRAG = 0.7f;
constexpr f32 OROGRAPHIC_DEFLECTION_FACTOR = 4.4f;
constexpr f32 MAX_OROGRAPHIC_DEFLECTION = 0.6f;
constexpr f32 MIN_WIND_SPEED_MPS = 1e-3f;
constexpr f32 MIN_GRADIENT_MAGNITUDE = 1e-4f;

constexpr f32 EPSILON_LATITUDE = 1e-4f;

f32 smoothStep(const f32 edge0, const f32 edge1, const f32 value) {
    if (value <= edge0) {
        return 0.0f;
    }
    if (value >= edge1) {
        return 1.0f;
    }

    const f32 normalized = (value - edge0) / (edge1 - edge0);
    return normalized * normalized * (3.0f - 2.0f * normalized);
}

f32 smoothWindow(const f32 value, const f32 start, const f32 end, const f32 fade) {
    const f32 rise = smoothStep(start - fade, start + fade, value);
    const f32 fall = 1.0f - smoothStep(end - fade, end + fade, value);
    return rise * fall;
}

f32 getHemisphereSign(const f32 latitudeRadians) {
    if (latitudeRadians > EPSILON_LATITUDE) {
        return 1.0f;
    }
    if (latitudeRadians < -EPSILON_LATITUDE) {
        return -1.0f;
    }
    return 0.0f;
}

f32 getSeasonalLatitudeShift(const f32 yearFraction) {
    // Shift the circulation belts as a simple proxy for seasonal ITCZ migration.
    return std::sin(static_cast<f32>(yearFraction * Astro::TWO_PI)) * MAX_SEASONAL_SHIFT_RADIANS;
}

u32 wrapColumn(const i32 column, const u32 width) {
    const i32 wrapped = column % static_cast<i32>(width);
    return static_cast<u32>(wrapped < 0 ? wrapped + static_cast<i32>(width) : wrapped);
}

u32 clampRow(const i32 row, const u32 height) {
    return static_cast<u32>(std::clamp(row, 0, static_cast<i32>(height - 1)));
}

f32 sampleRelativeAltitude(const ClimateState& climateState, const i32 column, const i32 row) {
    const u32 wrappedColumn = wrapColumn(column, climateState.gridWidth);
    const u32 clampedRow = clampRow(row, climateState.gridHeight);
    const u32 index = clampedRow * climateState.gridWidth + wrappedColumn;
    return climateState.relativeAltitude[index];
}

f32 computeAltitudeGradientX(const ClimateState& climateState, const u32 column, const u32 row) {
    const f32 leftHeight = sampleRelativeAltitude(climateState, static_cast<i32>(column) - 1, static_cast<i32>(row));
    const f32 rightHeight = sampleRelativeAltitude(climateState, static_cast<i32>(column) + 1, static_cast<i32>(row));
    return (rightHeight - leftHeight) / (2.0f * TILE_STEP_X);
}

f32 computeAltitudeGradientZ(const ClimateState& climateState, const u32 column, const u32 row) {
    const i32 signedColumn = static_cast<i32>(column);
    const i32 signedRow = static_cast<i32>(row);

    if (row == 0) {
        const f32 currentHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow);
        const f32 nextHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow + 1);
        return (nextHeight - currentHeight) / TILE_STEP_Z;
    }

    if (row + 1 >= climateState.gridHeight) {
        const f32 currentHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow);
        const f32 previousHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow - 1);
        return (currentHeight - previousHeight) / TILE_STEP_Z;
    }

    const f32 previousHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow - 1);
    const f32 nextHeight = sampleRelativeAltitude(climateState, signedColumn, signedRow + 1);
    return (nextHeight - previousHeight) / (2.0f * TILE_STEP_Z);
}

f32 vectorLength(const f32 x, const f32 z) {
    return std::sqrt(x * x + z * z);
}

void applyLatitudeCirculation(ClimateState& climateState) {
    if (!climateState.windEastMps || !climateState.windNorthMps || !climateState.latitudeRadians) {
        return;
    }

    const f32 seasonalShift = getSeasonalLatitudeShift(climateState.currentYearFraction);

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f32 shiftedLatitude = std::clamp(
            climateState.latitudeRadians[index] - seasonalShift,
            static_cast<f32>(-Astro::HALF_PI),
            static_cast<f32>(Astro::HALF_PI));
        const f32 absoluteLatitude = std::abs(shiftedLatitude);
        const f32 hemisphereSign = getHemisphereSign(shiftedLatitude);

        const f32 doldrumFade = smoothStep(CALM_EQUATOR_RADIANS * 0.5f, CALM_EQUATOR_RADIANS, absoluteLatitude);
        const f32 tradeWeight = smoothWindow(
            absoluteLatitude,
            TRADE_BAND_START_RADIANS,
            TRADE_BAND_END_RADIANS,
            BAND_FADE_RADIANS) * doldrumFade;
        const f32 westerlyWeight = smoothWindow(
            absoluteLatitude,
            WESTERLY_BAND_START_RADIANS,
            WESTERLY_BAND_END_RADIANS,
            BAND_FADE_RADIANS);
        const f32 polarWeight = smoothWindow(
            absoluteLatitude,
            POLAR_BAND_START_RADIANS,
            POLAR_BAND_END_RADIANS,
            BAND_FADE_RADIANS);

        climateState.windEastMps[index] =
            tradeWeight * TRADE_WIND_EAST_MPS
            + westerlyWeight * WESTERLY_WIND_EAST_MPS
            + polarWeight * POLAR_EASTERLY_WIND_EAST_MPS;

        climateState.windNorthMps[index] = hemisphereSign * (
            -tradeWeight * TRADE_WIND_MERIDIONAL_MPS
            + westerlyWeight * WESTERLY_WIND_MERIDIONAL_MPS
            - polarWeight * POLAR_EASTERLY_WIND_MERIDIONAL_MPS);
    }
}

void applyOrographicDrag(ClimateState& climateState) {
    if (!climateState.windEastMps || !climateState.windNorthMps || !climateState.relativeAltitude) {
        return;
    }

    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        for (u32 column = 0; column < climateState.gridWidth; ++column) {
            const u32 index = row * climateState.gridWidth + column;
            const f32 windEast = climateState.windEastMps[index];
            const f32 windNorth = climateState.windNorthMps[index];
            const f32 windSpeed = std::sqrt(windEast * windEast + windNorth * windNorth);
            if (windSpeed < MIN_WIND_SPEED_MPS) {
                continue;
            }

            const f32 gradientEast = computeAltitudeGradientX(climateState, column, row);
            const f32 gradientNorth = computeAltitudeGradientZ(climateState, column, row);
            const f32 directionEast = windEast / windSpeed;
            const f32 directionNorth = windNorth / windSpeed;

            // Positive dot product means the wind is climbing into higher terrain.
            const f32 upslope = std::max(
                0.0f,
                gradientEast * directionEast + gradientNorth * directionNorth);
            const f32 drag = std::clamp(upslope * OROGRAPHIC_DRAG_FACTOR, 0.0f, MAX_OROGRAPHIC_DRAG);
            const f32 retainedWindFactor = 1.0f - drag;

            climateState.windEastMps[index] = windEast * retainedWindFactor;
            climateState.windNorthMps[index] = windNorth * retainedWindFactor;
        }
    }
}

void applyOrographicDeflection(ClimateState& climateState) {
    if (!climateState.windEastMps || !climateState.windNorthMps || !climateState.relativeAltitude) {
        return;
    }

    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        for (u32 column = 0; column < climateState.gridWidth; ++column) {
            const u32 index = row * climateState.gridWidth + column;
            const f32 windEast = climateState.windEastMps[index];
            const f32 windNorth = climateState.windNorthMps[index];
            const f32 windSpeed = vectorLength(windEast, windNorth);
            if (windSpeed < MIN_WIND_SPEED_MPS) {
                continue;
            }

            const f32 gradientEast = computeAltitudeGradientX(climateState, column, row);
            const f32 gradientNorth = computeAltitudeGradientZ(climateState, column, row);
            const f32 gradientMagnitude = vectorLength(gradientEast, gradientNorth);
            if (gradientMagnitude < MIN_GRADIENT_MAGNITUDE) {
                continue;
            }

            const f32 directionEast = windEast / windSpeed;
            const f32 directionNorth = windNorth / windSpeed;
            const f32 upslope = std::max(
                0.0f,
                gradientEast * directionEast + gradientNorth * directionNorth);
            if (upslope <= 0.0f) {
                continue;
            }

            f32 tangentEast = -gradientNorth / gradientMagnitude;
            f32 tangentNorth = gradientEast / gradientMagnitude;
            const f32 tangentAlignment = tangentEast * directionEast + tangentNorth * directionNorth;
            if (tangentAlignment < 0.0f) {
                tangentEast = -tangentEast;
                tangentNorth = -tangentNorth;
            }

            const f32 deflection = std::clamp(
                upslope * OROGRAPHIC_DEFLECTION_FACTOR,
                0.0f,
                MAX_OROGRAPHIC_DEFLECTION);
            const f32 blendedEast = directionEast * (1.0f - deflection) + tangentEast * deflection;
            const f32 blendedNorth = directionNorth * (1.0f - deflection) + tangentNorth * deflection;
            const f32 blendedMagnitude = vectorLength(blendedEast, blendedNorth);
            if (blendedMagnitude < MIN_WIND_SPEED_MPS) {
                continue;
            }

            climateState.windEastMps[index] = windSpeed * blendedEast / blendedMagnitude;
            climateState.windNorthMps[index] = windSpeed * blendedNorth / blendedMagnitude;
        }
    }
}

} // namespace

void WindPass::initialize(ClimateState& climateState) {
    applyLatitudeCirculation(climateState);
    applyOrographicDrag(climateState);
    applyOrographicDeflection(climateState);
}

void WindPass::advanceOneTurn(ClimateState& climateState) {
    applyLatitudeCirculation(climateState);
    applyOrographicDrag(climateState);
    applyOrographicDeflection(climateState);
}