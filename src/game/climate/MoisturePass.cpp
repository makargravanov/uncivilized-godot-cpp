#include "MoisturePass.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Astrophysics.h"

namespace {

constexpr f32 KELVIN_OFFSET = 273.15f;

// Magnus-Tetens empirical coefficients for water vapor saturation curve.
constexpr f32 MAGNUS_COEFFICIENT_A = 17.27f;
constexpr f32 MAGNUS_COEFFICIENT_B = 237.3f;    // °C
constexpr f32 MAGNUS_BASE_PRESSURE_KPA = 0.6108f;

// Ratio of molecular mass of water to dry air.
constexpr f32 WATER_TO_DRY_AIR_MOLECULAR_RATIO = 0.622f;
constexpr f32 STANDARD_ATMOSPHERIC_PRESSURE_KPA = 101.325f;

// Evaporation from ocean tiles.
constexpr f32 OCEAN_EVAPORATION_RATE = 0.015f;

// Initial humidity as fraction of saturation.
constexpr f32 OCEAN_INITIAL_HUMIDITY_FRACTION = 0.80f;
constexpr f32 LAND_INITIAL_HUMIDITY_FRACTION = 0.20f;

// Number of advection sub-steps per game turn.
// Each sub-step moves moisture one tile in the wind direction.
constexpr u32 ADVECTION_SUB_STEPS = 12;

// Advection transfer fraction per sub-step (how much of the upwind cell's humidity is mixed in).
constexpr f32 ADVECTION_MIXING_FRACTION = 0.25f;

// Threshold below which relativeAltitude is considered ocean.
constexpr f32 OCEAN_ALTITUDE_THRESHOLD = 1e-6f;

// Overlay normalization range for humidity (kg/kg).
constexpr f32 MIN_OVERLAY_HUMIDITY = 0.0f;
constexpr f32 MAX_OVERLAY_HUMIDITY = 0.025f;

// Tile spacing (must match MapManager / WindPass values).
constexpr f32 TILE_STEP_X = 173.205078f / 100.0f;
constexpr f32 TILE_STEP_Z = 150.0f / 100.0f;

// ─── helpers ───────────────────────────────────────────────────────

f32 calculateSaturationVaporPressureKpa(f32 temperatureCelsius) {
    return MAGNUS_BASE_PRESSURE_KPA * std::exp(
        MAGNUS_COEFFICIENT_A * temperatureCelsius
        / (temperatureCelsius + MAGNUS_COEFFICIENT_B));
}

f32 calculateMaxSpecificHumidity(f32 temperatureCelsius) {
    const f32 saturationPressure = calculateSaturationVaporPressureKpa(temperatureCelsius);
    return WATER_TO_DRY_AIR_MOLECULAR_RATIO * saturationPressure
        / (STANDARD_ATMOSPHERIC_PRESSURE_KPA - saturationPressure);
}

bool isOceanTile(const ClimateState& climateState, u32 index) {
    return climateState.relativeAltitude[index] < OCEAN_ALTITUDE_THRESHOLD;
}

u32 wrapColumn(i32 column, u32 width) {
    const i32 wrapped = column % static_cast<i32>(width);
    return static_cast<u32>(wrapped < 0 ? wrapped + static_cast<i32>(width) : wrapped);
}

u32 clampRow(i32 row, u32 height) {
    return static_cast<u32>(std::clamp(row, 0, static_cast<i32>(height - 1)));
}

u32 tileIndex(const ClimateState& cs, i32 column, i32 row) {
    return clampRow(row, cs.gridHeight) * cs.gridWidth + wrapColumn(column, cs.gridWidth);
}

// ─── physics steps ─────────────────────────────────────────────────

void applyOceanEvaporation(ClimateState& climateState) {
    for (u32 i = 0; i < climateState.tileCount; ++i) {
        if (!isOceanTile(climateState, i)) continue;

        const f32 temperatureCelsius = climateState.temperatureKelvin[i] - KELVIN_OFFSET;
        const f32 maxHumidity = calculateMaxSpecificHumidity(temperatureCelsius);
        const f32 currentHumidity = climateState.humidityKgPerKg[i];
        const f32 humidityDeficit = std::max(0.0f, maxHumidity - currentHumidity);

        // Wind speed factor is 1.0 for now (intentionally not applied yet).
        constexpr f32 WIND_SPEED_FACTOR = 1.0f;

        climateState.humidityKgPerKg[i] += humidityDeficit * OCEAN_EVAPORATION_RATE * WIND_SPEED_FACTOR;
        climateState.humidityKgPerKg[i] = std::min(climateState.humidityKgPerKg[i], maxHumidity);
    }
}

void advectMoistureOneSubStep(ClimateState& climateState, const std::unique_ptr<f32[]>& previousHumidity) {
    const u32 width = climateState.gridWidth;

    for (u32 row = 0; row < climateState.gridHeight; ++row) {
        for (u32 col = 0; col < width; ++col) {
            const u32 idx = row * width + col;
            const f32 windEast = climateState.windEastMps[idx];
            const f32 windNorth = climateState.windNorthMps[idx];

            // Upwind: sample from the direction the wind is coming FROM.
            // Positive windEast means wind blows eastward → moisture comes from west (col-1).
            // Positive windNorth means wind blows northward → moisture comes from south (row+1 in screen coords).
            const i32 upwindCol = windEast > 0.0f
                ? static_cast<i32>(col) - 1
                : static_cast<i32>(col) + 1;
            const i32 upwindRow = windNorth > 0.0f
                ? static_cast<i32>(row) + 1
                : static_cast<i32>(row) - 1;

            const f32 windSpeed = std::sqrt(windEast * windEast + windNorth * windNorth);
            if (windSpeed < 1e-3f) {
                climateState.humidityKgPerKg[idx] = previousHumidity[idx];
                continue;
            }

            // Weight east/north contribution by the fraction of wind in that direction.
            const f32 eastWeight = std::abs(windEast) / windSpeed;
            const f32 northWeight = std::abs(windNorth) / windSpeed;

            const f32 upwindEastHumidity = previousHumidity[tileIndex(climateState, upwindCol, static_cast<i32>(row))];
            const f32 upwindNorthHumidity = previousHumidity[tileIndex(climateState, static_cast<i32>(col), upwindRow)];

            const f32 upwindBlended = eastWeight * upwindEastHumidity + northWeight * upwindNorthHumidity;
            const f32 currentHumidity = previousHumidity[idx];

            climateState.humidityKgPerKg[idx] =
                currentHumidity + (upwindBlended - currentHumidity) * ADVECTION_MIXING_FRACTION;
        }
    }
}

void advectMoisture(ClimateState& climateState) {
    auto previousHumidity = std::make_unique<f32[]>(climateState.tileCount);

    for (u32 step = 0; step < ADVECTION_SUB_STEPS; ++step) {
        std::memcpy(previousHumidity.get(), climateState.humidityKgPerKg.get(),
                     climateState.tileCount * sizeof(f32));
        advectMoistureOneSubStep(climateState, previousHumidity);
    }
}

void applyCondensationAndPrecipitation(ClimateState& climateState) {
    for (u32 i = 0; i < climateState.tileCount; ++i) {
        const f32 temperatureCelsius = climateState.temperatureKelvin[i] - KELVIN_OFFSET;
        const f32 maxHumidity = calculateMaxSpecificHumidity(temperatureCelsius);

        f32 excess = climateState.humidityKgPerKg[i] - maxHumidity;
        if (excess > 0.0f) {
            climateState.humidityKgPerKg[i] = maxHumidity;
            climateState.turnPrecipitation[i] += excess;
        }
    }
}

void accumulateAnnualPrecipitation(ClimateState& climateState) {
    const u32 turnInYear = climateState.currentTurnIndex;

    if (turnInYear == 0) {
        // New year — reset the accumulator.
        std::memset(climateState.annualPrecipitationAccumulator.get(), 0,
                     climateState.tileCount * sizeof(f32));
    }

    for (u32 i = 0; i < climateState.tileCount; ++i) {
        climateState.annualPrecipitationAccumulator[i] += climateState.turnPrecipitation[i];
    }
}

} // namespace

void MoisturePass::initialize(ClimateState& climateState) {
    if (!climateState.humidityKgPerKg || !climateState.temperatureKelvin) return;

    for (u32 i = 0; i < climateState.tileCount; ++i) {
        const f32 temperatureCelsius = climateState.temperatureKelvin[i] - KELVIN_OFFSET;
        const f32 maxHumidity = calculateMaxSpecificHumidity(temperatureCelsius);

        const f32 fraction = isOceanTile(climateState, i)
            ? OCEAN_INITIAL_HUMIDITY_FRACTION
            : LAND_INITIAL_HUMIDITY_FRACTION;

        climateState.humidityKgPerKg[i] = maxHumidity * fraction;
    }

    std::memset(climateState.turnPrecipitation.get(), 0, climateState.tileCount * sizeof(f32));
    std::memset(climateState.annualPrecipitationAccumulator.get(), 0, climateState.tileCount * sizeof(f32));

    // Run several cycles to reach a plausible initial distribution.
    for (u32 warmup = 0; warmup < 4; ++warmup) {
        applyOceanEvaporation(climateState);
        advectMoisture(climateState);
        applyCondensationAndPrecipitation(climateState);
    }

    // Clear warmup artifacts, then compute one clean initial precipitation snapshot.
    std::memset(climateState.turnPrecipitation.get(), 0, climateState.tileCount * sizeof(f32));
    std::memset(climateState.annualPrecipitationAccumulator.get(), 0, climateState.tileCount * sizeof(f32));

    applyOceanEvaporation(climateState);
    advectMoisture(climateState);
    applyCondensationAndPrecipitation(climateState);
    accumulateAnnualPrecipitation(climateState);
}

void MoisturePass::advanceOneTurn(ClimateState& climateState) {
    if (!climateState.humidityKgPerKg || !climateState.temperatureKelvin ||
        !climateState.windEastMps || !climateState.windNorthMps) return;

    std::memset(climateState.turnPrecipitation.get(), 0, climateState.tileCount * sizeof(f32));

    applyOceanEvaporation(climateState);
    advectMoisture(climateState);
    applyCondensationAndPrecipitation(climateState);
    accumulateAnnualPrecipitation(climateState);
}

void MoisturePass::publishToTiles(const ClimateState& climateState, const std::unique_ptr<TileData[]>& tiles) {
    if (!tiles || !climateState.humidityKgPerKg) return;

    for (u32 i = 0; i < climateState.tileCount; ++i) {
        // Normalize to 0..255 for compact TileData storage.
        const f32 normalized = std::clamp(
            climateState.humidityKgPerKg[i] / MAX_OVERLAY_HUMIDITY, 0.0f, 1.0f);
        tiles[i].humidity = static_cast<u8>(normalized * 255.0f);
    }
}

f32 MoisturePass::normalizeForOverlay(f32 humidityKgPerKg) {
    return std::clamp(
        (humidityKgPerKg - MIN_OVERLAY_HUMIDITY) / (MAX_OVERLAY_HUMIDITY - MIN_OVERLAY_HUMIDITY),
        0.0f, 1.0f);
}
