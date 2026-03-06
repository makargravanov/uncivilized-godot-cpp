//
// Created by Copilot on 06.03.2026.
//

#include "Astrophysics.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr f64 ONE_AU_METERS = 149597870700.0;
constexpr f64 EPSILON = 1e-12;

u32 toEvenSegmentCount(u32 segments) {
    if (segments < 2) {
        segments = 2;
    }
    if ((segments & 1u) != 0u) {
        ++segments;
    }
    return segments;
}

f64 clampLatitude(f64 latitudeRadians) {
    return std::clamp(latitudeRadians, -Astro::HALF_PI, Astro::HALF_PI);
}

f64 integrateSegmentAverage(
    const f64 latitudeRadians,
    const f64 startYearFraction,
    const f64 durationYearFraction,
    const Astro::StarParams& starParams,
    const Astro::OrbitalParams& orbitalParams,
    u32 integrationSegments)
{
    if (durationYearFraction <= EPSILON) {
        return 0.0;
    }

    integrationSegments = toEvenSegmentCount(integrationSegments);

    const f64 solarConstant = Astro::Astrophysics::calculateSolarConstantAtMeanOrbit(starParams, orbitalParams);
    const f64 latitude = clampLatitude(latitudeRadians);
    const f64 step = 1.0 / static_cast<f64>(integrationSegments);

    auto sample = [&](const f64 localT) -> f64 {
        const auto state = Astro::Astrophysics::calculateOrbitStateByYearFraction(
            orbitalParams,
            startYearFraction + durationYearFraction * localT);
        return Astro::Astrophysics::calculateDailyAverageInsolation(state, latitude, solarConstant);
    };

    f64 weightedSum = sample(0.0) + sample(1.0);
    for (u32 index = 1; index < integrationSegments; ++index) {
        const f64 weight = (index % 2u == 0u) ? 2.0 : 4.0;
        weightedSum += weight * sample(static_cast<f64>(index) * step);
    }

    return (weightedSum * step) / 3.0;
}

} // namespace

namespace Astro {

f64 StarParams::solarConstantAt1AU() const {
    return luminosity / (4.0 * PI * ONE_AU_METERS * ONE_AU_METERS);
}

f64 Astrophysics::calculateSolarConstantAtMeanOrbit(
    const StarParams& starParams,
    const OrbitalParams& orbitalParams)
{
    const f64 semiMajorAxis = std::max(orbitalParams.semiMajorAxisAU, EPSILON);
    return starParams.solarConstantAt1AU() / (semiMajorAxis * semiMajorAxis);
}

InstantOrbitState Astrophysics::calculateOrbitState(const OrbitalParams& params, const f64 timeDays) {
    const f64 orbitalPeriod = std::max(params.orbitalPeriodDay, EPSILON);
    const f64 meanMotion = TWO_PI / orbitalPeriod;

    f64 meanAnomaly = std::fmod(meanMotion * timeDays, TWO_PI);
    if (meanAnomaly < 0.0) {
        meanAnomaly += TWO_PI;
    }

    const f64 eccentricity = std::clamp(params.eccentricity, 0.0, 0.999999);
    f64 eccentricAnomaly = meanAnomaly;

    for (u32 iteration = 0; iteration < 10; ++iteration) {
        const f64 delta = eccentricAnomaly - eccentricity * std::sin(eccentricAnomaly) - meanAnomaly;
        const f64 derivative = 1.0 - eccentricity * std::cos(eccentricAnomaly);
        eccentricAnomaly -= delta / derivative;
    }

    const f64 denominator = 1.0 - eccentricity * std::cos(eccentricAnomaly);
    const f64 cosNu = (std::cos(eccentricAnomaly) - eccentricity) / denominator;
    const f64 sinNu = (std::sqrt(1.0 - eccentricity * eccentricity) * std::sin(eccentricAnomaly)) / denominator;

    f64 trueAnomaly = std::atan2(sinNu, cosNu);
    if (trueAnomaly < 0.0) {
        trueAnomaly += TWO_PI;
    }

    const f64 distanceRatio = (1.0 + eccentricity * std::cos(trueAnomaly)) / (1.0 - eccentricity * eccentricity);
    const f64 distanceFactor = distanceRatio * distanceRatio;

    const f64 eclipticLongitude = trueAnomaly + params.longitudeOfPerihelion;
    const f64 sinDeclination = std::sin(params.obliquity) * std::sin(eclipticLongitude);
    const f64 declination = std::asin(std::clamp(sinDeclination, -1.0, 1.0));

    return { distanceFactor, declination };
}

InstantOrbitState Astrophysics::calculateOrbitStateByYearFraction(const OrbitalParams& params, const f64 yearFraction) {
    const f64 normalizedFraction = normalizeYearFraction(yearFraction);
    return calculateOrbitState(params, normalizedFraction * params.orbitalPeriodDay);
}

f64 Astrophysics::calculateDailyAverageInsolation(
    const InstantOrbitState& state,
    f64 latitudeRadians,
    const f64 solarConstant)
{
    latitudeRadians = clampLatitude(latitudeRadians);

    const f64 x = -std::tan(latitudeRadians) * std::tan(state.declination);
    f64 hourAngle = 0.0;

    if (x >= 1.0) {
        hourAngle = 0.0;
    } else if (x <= -1.0) {
        hourAngle = PI;
    } else {
        hourAngle = std::acos(std::clamp(x, -1.0, 1.0));
    }

    const f64 term1 = hourAngle * std::sin(latitudeRadians) * std::sin(state.declination);
    const f64 term2 = std::cos(latitudeRadians) * std::cos(state.declination) * std::sin(hourAngle);
    const f64 averageCosZenith = std::max(0.0, (term1 + term2) / PI);

    return solarConstant * state.distanceFactor * averageCosZenith;
}

f64 Astrophysics::calculateAverageInsolationForYearFractionRange(
    const f64 latitudeRadians,
    const f64 startYearFraction,
    const f64 endYearFraction,
    const StarParams& starParams,
    const OrbitalParams& orbitalParams,
    const u32 integrationSegments)
{
    f64 duration = endYearFraction - startYearFraction;
    if (std::abs(duration) <= EPSILON) {
        return 0.0;
    }
    if (duration < 0.0) {
        duration += std::ceil(-duration);
    }

    return calculateAverageInsolationForDuration(
        latitudeRadians,
        startYearFraction,
        duration,
        starParams,
        orbitalParams,
        integrationSegments);
}

f64 Astrophysics::calculateAverageInsolationForTurn(
    const f64 latitudeRadians,
    const u32 turnIndex,
    const StarParams& starParams,
    const OrbitalParams& orbitalParams,
    const u32 turnsPerYear,
    const u32 integrationSegmentsPerTurn)
{
    return calculateAverageInsolationForTurnRange(
        latitudeRadians,
        turnIndex,
        turnIndex + 1,
        starParams,
        orbitalParams,
        turnsPerYear,
        integrationSegmentsPerTurn);
}

f64 Astrophysics::calculateAverageInsolationForTurnRange(
    const f64 latitudeRadians,
    const u32 startTurn,
    const u32 endTurnExclusive,
    const StarParams& starParams,
    const OrbitalParams& orbitalParams,
    const u32 turnsPerYear,
    const u32 integrationSegmentsPerTurn)
{
    if (turnsPerYear == 0) {
        return 0.0;
    }

    const f64 startFraction = static_cast<f64>(startTurn) / static_cast<f64>(turnsPerYear);
    const f64 endFraction = static_cast<f64>(endTurnExclusive) / static_cast<f64>(turnsPerYear);

    f64 durationTurns = static_cast<f64>(endTurnExclusive) - static_cast<f64>(startTurn);
    if (durationTurns < 0.0) {
        durationTurns += std::ceil(-durationTurns / static_cast<f64>(turnsPerYear)) * static_cast<f64>(turnsPerYear);
    }

    const u32 segmentCount = toEvenSegmentCount(
        static_cast<u32>(std::ceil(std::max(1.0, durationTurns) * static_cast<f64>(integrationSegmentsPerTurn))));

    return calculateAverageInsolationForYearFractionRange(
        latitudeRadians,
        startFraction,
        endFraction,
        starParams,
        orbitalParams,
        segmentCount);
}

f64 Astrophysics::normalizeYearFraction(f64 yearFraction) {
    yearFraction = std::fmod(yearFraction, 1.0);
    if (yearFraction < 0.0) {
        yearFraction += 1.0;
    }
    return yearFraction;
}

f64 Astrophysics::calculateAverageInsolationForDuration(
    const f64 latitudeRadians,
    const f64 startYearFraction,
    const f64 durationYearFraction,
    const StarParams& starParams,
    const OrbitalParams& orbitalParams,
    const u32 integrationSegments)
{
    if (durationYearFraction <= EPSILON) {
        return 0.0;
    }

    const f64 normalizedStart = normalizeYearFraction(startYearFraction);

    if (durationYearFraction >= 1.0 - EPSILON) {
        const f64 wholeYears = std::floor(durationYearFraction);
        const f64 remainder = durationYearFraction - wholeYears;
        const f64 annualAverage = integrateSegmentAverage(
            latitudeRadians,
            0.0,
            1.0,
            starParams,
            orbitalParams,
            integrationSegments);

        if (remainder <= EPSILON) {
            return annualAverage;
        }

        const f64 remainderAverage = calculateAverageInsolationForDuration(
            latitudeRadians,
            normalizedStart,
            remainder,
            starParams,
            orbitalParams,
            integrationSegments);

        return ((annualAverage * wholeYears) + (remainderAverage * remainder)) / durationYearFraction;
    }

    if (normalizedStart + durationYearFraction <= 1.0 + EPSILON) {
        return integrateSegmentAverage(
            latitudeRadians,
            normalizedStart,
            durationYearFraction,
            starParams,
            orbitalParams,
            integrationSegments);
    }

    const f64 tailDuration = 1.0 - normalizedStart;
    const f64 headDuration = durationYearFraction - tailDuration;

    const f64 tailAverage = integrateSegmentAverage(
        latitudeRadians,
        normalizedStart,
        tailDuration,
        starParams,
        orbitalParams,
        integrationSegments);
    const f64 headAverage = integrateSegmentAverage(
        latitudeRadians,
        0.0,
        headDuration,
        starParams,
        orbitalParams,
        integrationSegments);

    return ((tailAverage * tailDuration) + (headAverage * headDuration)) / durationYearFraction;
}

} // namespace Astro