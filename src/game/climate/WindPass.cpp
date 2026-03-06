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

} // namespace

void WindPass::initialize(ClimateState& climateState) {
    applyLatitudeCirculation(climateState);
}

void WindPass::advanceOneTurn(ClimateState& climateState) {
    applyLatitudeCirculation(climateState);
}