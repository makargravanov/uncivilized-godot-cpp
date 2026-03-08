//
// Created by Copilot on 06.03.2026.
//

#include "WindPass.h"

#include <algorithm>
#include <cmath>

#include "Astrophysics.h"
#include "ClimateConfig.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

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
    if (latitudeRadians > CONFIG.wind.equatorLatitudeEpsilonRadians) {
        return 1.0f;
    }
    if (latitudeRadians < -CONFIG.wind.equatorLatitudeEpsilonRadians) {
        return -1.0f;
    }
    return 0.0f;
}

f32 getSeasonalLatitudeShift(const f32 yearFraction) {
    const Astro::OrbitalParams orbitalParams;
    const auto orbitState = Astro::Astrophysics::calculateOrbitStateByYearFraction(
        orbitalParams, static_cast<f64>(yearFraction));
    return static_cast<f32>(orbitState.declination) * CONFIG.wind.itczDeclinationTrackingFraction;
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

f32 vectorLength(const f32 x, const f32 z) {
    return std::sqrt(x * x + z * z);
}

void applyLatitudeCirculation(ClimateState& climateState) {
    if (!climateState.windEastMps || !climateState.windNorthMps || !climateState.latitudeRadians) {
        return;
    }

    const f32 seasonalShift = getSeasonalLatitudeShift(climateState.currentYearFraction);
    const f32 calmEquatorRadians = ClimateSettings::toRadians(CONFIG.wind.calmEquatorDegrees);
    const f32 bandFadeRadians = ClimateSettings::toRadians(CONFIG.wind.bandFadeDegrees);
    const f32 tradeBandStartRadians = ClimateSettings::toRadians(CONFIG.wind.tradeBandStartDegrees);
    const f32 tradeBandEndRadians = ClimateSettings::toRadians(CONFIG.wind.tradeBandEndDegrees);
    const f32 westerlyBandStartRadians = ClimateSettings::toRadians(CONFIG.wind.westerlyBandStartDegrees);
    const f32 westerlyBandEndRadians = ClimateSettings::toRadians(CONFIG.wind.westerlyBandEndDegrees);
    const f32 polarBandStartRadians = ClimateSettings::toRadians(CONFIG.wind.polarBandStartDegrees);
    const f32 polarBandEndRadians = ClimateSettings::toRadians(CONFIG.wind.polarBandEndDegrees);

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f32 shiftedLatitude = std::clamp(
            climateState.latitudeRadians[index] - seasonalShift,
            static_cast<f32>(-Astro::HALF_PI),
            static_cast<f32>(Astro::HALF_PI));
        const f32 absoluteLatitude = std::abs(shiftedLatitude);
        const f32 hemisphereSign = getHemisphereSign(shiftedLatitude);

        const f32 doldrumFade = smoothStep(calmEquatorRadians * 0.5f, calmEquatorRadians, absoluteLatitude);
        const f32 tradeWeight = smoothWindow(
            absoluteLatitude,
            tradeBandStartRadians,
            tradeBandEndRadians,
            bandFadeRadians) * doldrumFade;
        const f32 westerlyWeight = smoothWindow(
            absoluteLatitude,
            westerlyBandStartRadians,
            westerlyBandEndRadians,
            bandFadeRadians);
        const f32 polarWeight = smoothWindow(
            absoluteLatitude,
            polarBandStartRadians,
            polarBandEndRadians,
            bandFadeRadians);

        climateState.windEastMps[index] =
            tradeWeight * CONFIG.wind.tradeWindEastMps
            + westerlyWeight * CONFIG.wind.westerlyWindEastMps
            + polarWeight * CONFIG.wind.polarEasterlyWindEastMps;

        climateState.windNorthMps[index] = hemisphereSign * (
            -tradeWeight * CONFIG.wind.tradeWindMeridionalMps
            + westerlyWeight * CONFIG.wind.westerlyWindMeridionalMps
            - polarWeight * CONFIG.wind.polarEasterlyWindMeridionalMps);
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
            if (windSpeed < CONFIG.shared.minWindSpeedMps) {
                continue;
            }

            const f32 gradientEast = computeAltitudeGradientX(climateState, column, row);
            // computeAltitudeGradientZ returns gradient in +row (south) direction;
            // negate to get the northward gradient matching windNorth convention.
            const f32 gradientNorth = -computeAltitudeGradientZ(climateState, column, row);
            const f32 directionEast = windEast / windSpeed;
            const f32 directionNorth = windNorth / windSpeed;

            // Positive dot product means the wind is climbing into higher terrain.
            const f32 upslope = std::max(
                0.0f,
                gradientEast * directionEast + gradientNorth * directionNorth);
            const f32 drag = std::clamp(
                upslope * CONFIG.wind.orographicDragFactor,
                0.0f,
                CONFIG.wind.maxOrographicDrag);
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
            if (windSpeed < CONFIG.shared.minWindSpeedMps) {
                continue;
            }

            const f32 gradientEast = computeAltitudeGradientX(climateState, column, row);
            const f32 gradientNorth = -computeAltitudeGradientZ(climateState, column, row);
            const f32 gradientMagnitude = vectorLength(gradientEast, gradientNorth);
            if (gradientMagnitude < CONFIG.wind.minGradientMagnitude) {
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
                upslope * CONFIG.wind.orographicDeflectionFactor,
                0.0f,
                CONFIG.wind.maxOrographicDeflection);
            const f32 blendedEast = directionEast * (1.0f - deflection) + tangentEast * deflection;
            const f32 blendedNorth = directionNorth * (1.0f - deflection) + tangentNorth * deflection;
            const f32 blendedMagnitude = vectorLength(blendedEast, blendedNorth);
            if (blendedMagnitude < CONFIG.shared.minWindSpeedMps) {
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