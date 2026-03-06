//
// Created by Copilot on 06.03.2026.
//

#ifndef ASTROPHYSICS_H
#define ASTROPHYSICS_H

#include "util/declarations.h"

namespace Astro {

constexpr f64 PI         = 3.14159265358979323846264338327950288;
constexpr f64 TWO_PI     = 2.0 * PI;
constexpr f64 HALF_PI    = 0.5 * PI;
constexpr f64 DEG_TO_RAD = PI / 180.0;

constexpr u32 DEFAULT_YEAR_TURN_COUNT = 48;

struct StarParams {
    f64 mass = 1.0;
    f64 luminosity = 3.828e26;

    f64 solarConstantAt1AU() const;
};

struct OrbitalParams {
    f64 semiMajorAxisAU = 1.0;
    f64 eccentricity = 0.0167;
    f64 obliquity = 23.44 * DEG_TO_RAD;
    f64 longitudeOfPerihelion = 283.0 * DEG_TO_RAD;
    f64 rotationalPeriodDay = 1.0;
    f64 orbitalPeriodDay = 365.256;
};

struct InstantOrbitState {
    f64 distanceFactor = 1.0;
    f64 declination = 0.0;
};

class Astrophysics {
public:
    static f64 calculateSolarConstantAtMeanOrbit(
        const StarParams& starParams = StarParams(),
        const OrbitalParams& orbitalParams = OrbitalParams());

    static InstantOrbitState calculateOrbitState(const OrbitalParams& params, f64 timeDays);
    static InstantOrbitState calculateOrbitStateByYearFraction(const OrbitalParams& params, f64 yearFraction);

    static f64 calculateDailyAverageInsolation(
        const InstantOrbitState& state,
        f64 latitudeRadians,
        f64 solarConstant);

    // Average insolation for the cyclic interval [start, end).
    // Fractions are expressed in years: 3/48 .. 4/48 is one game turn.
    static f64 calculateAverageInsolationForYearFractionRange(
        f64 latitudeRadians,
        f64 startYearFraction,
        f64 endYearFraction,
        const StarParams& starParams = StarParams(),
        const OrbitalParams& orbitalParams = OrbitalParams(),
        u32 integrationSegments = 32);

    static f64 calculateAverageInsolationForTurn(
        f64 latitudeRadians,
        u32 turnIndex,
        const StarParams& starParams = StarParams(),
        const OrbitalParams& orbitalParams = OrbitalParams(),
        u32 turnsPerYear = DEFAULT_YEAR_TURN_COUNT,
        u32 integrationSegmentsPerTurn = 8);

    static f64 calculateAverageInsolationForTurnRange(
        f64 latitudeRadians,
        u32 startTurn,
        u32 endTurnExclusive,
        const StarParams& starParams = StarParams(),
        const OrbitalParams& orbitalParams = OrbitalParams(),
        u32 turnsPerYear = DEFAULT_YEAR_TURN_COUNT,
        u32 integrationSegmentsPerTurn = 8);

private:
    static f64 normalizeYearFraction(f64 yearFraction);
    static f64 calculateAverageInsolationForDuration(
        f64 latitudeRadians,
        f64 startYearFraction,
        f64 durationYearFraction,
        const StarParams& starParams,
        const OrbitalParams& orbitalParams,
        u32 integrationSegments);
};

} // namespace Astro

#endif //ASTROPHYSICS_H