#include "BiomeClassifier.h"

#include <algorithm>
#include <cmath>

#include "FeatureType.h"
#include "game/climate/ClimateConfig.h"
#include "game/climate/ClimateState.h"

namespace {

constexpr f32 KELVIN_OFFSET = ClimateSettings::DEFAULT_CLIMATE_CONFIG.shared.kelvinOffset;
constexpr f32 CLIMATE_EPSILON = 1e-4f;
constexpr f32 ALPINE_ANNUAL_MEAN_TEMPERATURE_C = -1.0f;
constexpr f32 ALPINE_WARMEST_QUARTER_TEMPERATURE_C = 10.0f;
constexpr f32 POLAR_DESERT_WARMEST_QUARTER_C = 3.0f;
constexpr f32 TUNDRA_WARMEST_QUARTER_C = 10.0f;
constexpr f32 COLD_CLIMATE_ANNUAL_MEAN_C = 3.0f;
constexpr f32 COLD_CLIMATE_COLDEST_QUARTER_C = -12.0f;
constexpr f32 HOT_CLIMATE_ANNUAL_MEAN_C = 18.0f;
constexpr f32 HOT_CLIMATE_WARMEST_QUARTER_C = 24.0f;
constexpr f32 TROPICAL_COLDEST_QUARTER_C = 18.0f;
constexpr f32 HOT_DESERT_WARMEST_QUARTER_C = 26.0f;
constexpr f32 WARM_SAVANNA_COLDEST_QUARTER_C = 8.0f;
constexpr f32 WARM_SEASONAL_FOREST_COLDEST_QUARTER_C = 10.0f;
constexpr f32 FOREST_MIN_DRY_QUARTER_SHARE = 0.10f;
constexpr f32 STRONG_DRY_SEASON_SHARE = 0.06f;
constexpr f32 SEASONAL_PRECIPITATION_CONTRAST = 6.0f;
constexpr f32 STRONG_PRECIPITATION_CONTRAST = 9.0f;
constexpr f32 POLAR_DESERT_MAX_ANNUAL_PRECIPITATION = 0.18f;
constexpr f32 TUNDRA_MAX_ANNUAL_PRECIPITATION = 0.22f;
constexpr f32 COLD_STEPPE_MAX_ANNUAL_PRECIPITATION = 0.34f;
constexpr f32 TEMPERATE_FOREST_MIN_ANNUAL_PRECIPITATION = 0.55f;
constexpr f32 TROPICAL_SEASONAL_FOREST_MIN_ANNUAL_PRECIPITATION = 0.72f;
constexpr f32 TROPICAL_RAINFOREST_MIN_ANNUAL_PRECIPITATION = 0.95f;

struct ClimateBiomeInputs {
    f32 annualMeanTemperatureC;
    f32 coldestQuarterMeanTemperatureC;
    f32 warmestQuarterMeanTemperatureC;
    f32 annualPrecipitation;
    f32 driestQuarterPrecipitation;
    f32 wettestQuarterPrecipitation;
};

bool isMountainRelief(const ReliefType relief) {
    return relief == RELIEF_MOUNTAIN || relief == RELIEF_HIGH_MOUNTAIN;
}

f32 normalizedEquatorDistance(const u32 row, const u32 height) {
    const f32 normalizedRow = (static_cast<f32>(row) + 0.5f) / static_cast<f32>(std::max(height, 1u));
    return std::abs(normalizedRow * 2.0f - 1.0f);
}

BiomeType classifyInitialLandBiome(const u32 row, const u32 height) {
    const f32 equatorDistance = normalizedEquatorDistance(row, height);

    if (equatorDistance >= 0.84f) {
        return BIOME_POLAR_DESERT;
    }
    if (equatorDistance >= 0.72f) {
        return BIOME_TUNDRA;
    }
    if (equatorDistance >= 0.54f) {
        return BIOME_BOREAL_FOREST;
    }
    if (equatorDistance >= 0.28f) {
        return BIOME_TEMPERATE_FOREST;
    }
    if (equatorDistance >= 0.16f) {
        return BIOME_XERIC_SHRUBLAND;
    }
    if (equatorDistance >= 0.08f) {
        return BIOME_SAVANNA;
    }
    return BIOME_TROPICAL_SEASONAL_FOREST;
}

BiomeType classifyInitialBiome(const DiscreteLandTypeByHeight type, const u32 row, const u32 height) {
    switch (type) {
        case OCEAN:
            return BIOME_DEEP_OCEAN;
        case SHALLOW_OCEAN:
            return BIOME_SHALLOW_OCEAN;
        case VALLEY:
            return BIOME_INLAND_SEA;
        case SHALLOW_VALLEY:
            return BIOME_SHALLOW_INLAND_SEA;
        case PLAIN:
        case HILL:
            return classifyInitialLandBiome(row, height);
        case MOUNTAIN:
        case HIGH_MOUNTAIN:
            return classifyInitialLandBiome(row, height);
        default:
            return classifyInitialLandBiome(row, height);
    }
}

f32 computeDryClimateThreshold(
    const f32 annualMeanTemperatureC,
    const f32 warmestQuarterMeanTemperatureC) {
    const f32 heatLoad =
        std::max(annualMeanTemperatureC, 0.0f)
        + std::max(warmestQuarterMeanTemperatureC - 10.0f, 0.0f) * 0.45f;
    return 0.11f + heatLoad * 0.0085f;
}

BiomeType classifyClimateLandBiome(const ReliefType relief, const ClimateBiomeInputs& climate) {
    const f32 annualPrecipitation = std::max(climate.annualPrecipitation, 0.0f);
    const f32 driestQuarterPrecipitation = std::max(climate.driestQuarterPrecipitation, 0.0f);
    const f32 wettestQuarterPrecipitation =
        std::max(climate.wettestQuarterPrecipitation, driestQuarterPrecipitation);
    const f32 driestQuarterShare = annualPrecipitation > CLIMATE_EPSILON
        ? driestQuarterPrecipitation / annualPrecipitation
        : 0.0f;
    const f32 wetDryContrast = wettestQuarterPrecipitation
        / std::max(driestQuarterPrecipitation, CLIMATE_EPSILON);

    const f32 desertThreshold = computeDryClimateThreshold(
        climate.annualMeanTemperatureC,
        climate.warmestQuarterMeanTemperatureC);
    const f32 xericThreshold = desertThreshold * 1.45f;
    const f32 steppeThreshold = desertThreshold * 2.0f;

    const bool desertDry = annualPrecipitation < desertThreshold;
    const bool xericDry = annualPrecipitation < xericThreshold;
    const bool steppeDry = annualPrecipitation < steppeThreshold;
    const bool seasonalDryness =
        driestQuarterShare < FOREST_MIN_DRY_QUARTER_SHARE
        || wetDryContrast > SEASONAL_PRECIPITATION_CONTRAST;
    const bool strongDrySeason =
        driestQuarterShare < STRONG_DRY_SEASON_SHARE
        || wetDryContrast > STRONG_PRECIPITATION_CONTRAST;

    const bool tropical = climate.coldestQuarterMeanTemperatureC >= TROPICAL_COLDEST_QUARTER_C;
    const bool cold = climate.annualMeanTemperatureC <= COLD_CLIMATE_ANNUAL_MEAN_C
        || climate.coldestQuarterMeanTemperatureC <= COLD_CLIMATE_COLDEST_QUARTER_C;
    const bool hot = climate.annualMeanTemperatureC >= HOT_CLIMATE_ANNUAL_MEAN_C
        || climate.warmestQuarterMeanTemperatureC >= HOT_CLIMATE_WARMEST_QUARTER_C;

    if (isMountainRelief(relief)
        && (climate.annualMeanTemperatureC <= ALPINE_ANNUAL_MEAN_TEMPERATURE_C
            || climate.warmestQuarterMeanTemperatureC <= ALPINE_WARMEST_QUARTER_TEMPERATURE_C)) {
        return BIOME_ALPINE;
    }

    if (climate.warmestQuarterMeanTemperatureC <= POLAR_DESERT_WARMEST_QUARTER_C) {
        return annualPrecipitation < POLAR_DESERT_MAX_ANNUAL_PRECIPITATION
            ? BIOME_POLAR_DESERT
            : BIOME_TUNDRA;
    }

    if (climate.warmestQuarterMeanTemperatureC <= TUNDRA_WARMEST_QUARTER_C) {
        return annualPrecipitation < TUNDRA_MAX_ANNUAL_PRECIPITATION
            ? BIOME_POLAR_DESERT
            : BIOME_TUNDRA;
    }

    if (cold) {
        if (steppeDry || annualPrecipitation < COLD_STEPPE_MAX_ANNUAL_PRECIPITATION) {
            return BIOME_COLD_STEPPE;
        }
        return BIOME_BOREAL_FOREST;
    }

    if (tropical) {
        if (desertDry) {
            return BIOME_HOT_DESERT;
        }
        if (steppeDry || strongDrySeason) {
            return BIOME_SAVANNA;
        }
        if (seasonalDryness || annualPrecipitation < TROPICAL_RAINFOREST_MIN_ANNUAL_PRECIPITATION) {
            return BIOME_TROPICAL_SEASONAL_FOREST;
        }
        return BIOME_TROPICAL_RAINFOREST;
    }

    if (hot) {
        if (desertDry && climate.warmestQuarterMeanTemperatureC >= HOT_DESERT_WARMEST_QUARTER_C) {
            return BIOME_HOT_DESERT;
        }
        if (xericDry) {
            return BIOME_XERIC_SHRUBLAND;
        }
        if (steppeDry) {
            if (climate.coldestQuarterMeanTemperatureC >= WARM_SAVANNA_COLDEST_QUARTER_C && strongDrySeason) {
                return BIOME_SAVANNA;
            }
            return BIOME_TEMPERATE_STEPPE;
        }
        if (seasonalDryness
            && climate.coldestQuarterMeanTemperatureC >= WARM_SEASONAL_FOREST_COLDEST_QUARTER_C
            && annualPrecipitation >= TROPICAL_SEASONAL_FOREST_MIN_ANNUAL_PRECIPITATION) {
            return BIOME_TROPICAL_SEASONAL_FOREST;
        }
        if (annualPrecipitation >= TEMPERATE_FOREST_MIN_ANNUAL_PRECIPITATION) {
            return BIOME_TEMPERATE_FOREST;
        }
        return BIOME_TEMPERATE_STEPPE;
    }

    if (desertDry || xericDry) {
        return BIOME_XERIC_SHRUBLAND;
    }

    if (steppeDry || annualPrecipitation < TEMPERATE_FOREST_MIN_ANNUAL_PRECIPITATION) {
        return BIOME_TEMPERATE_STEPPE;
    }

    return BIOME_TEMPERATE_FOREST;
}

FeatureFlags updateBiomeFeatures(const FeatureFlags existingFeatures, const BiomeType biome) {
    FeatureFlags updatedFeatures = existingFeatures & ~Feature::HAS_FOREST;
    if (isForestBiome(biome)) {
        updatedFeatures |= Feature::HAS_FOREST;
    }
    return updatedFeatures;
}

} // namespace

std::unique_ptr<TileData[]> BiomeClassifier::classify(
    const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
    const u32 width,
    const u32 height) {
    const u32 total = width * height;
    auto tiles = std::make_unique<TileData[]>(total);

    for (u32 i = 0; i < total; ++i) {
        const u32 row = i / width;
        const BiomeType biome = classifyInitialBiome(discrete[i], row, height);
        switch (discrete[i]) {
            case OCEAN:
            case SHALLOW_OCEAN:
            case VALLEY:
            case SHALLOW_VALLEY:
                tiles[i] = { RELIEF_OCEAN, biome };
                break;
            case PLAIN:
                tiles[i] = { RELIEF_LAND, biome };
                break;
            case HILL:
                tiles[i] = { RELIEF_HILL, biome };
                break;
            case MOUNTAIN:
                tiles[i] = { RELIEF_MOUNTAIN, biome };
                break;
            case HIGH_MOUNTAIN:
                tiles[i] = { RELIEF_HIGH_MOUNTAIN, biome };
                break;
            default:
                tiles[i] = { RELIEF_LAND, classifyInitialLandBiome(row, height) };
                break;
        }
    }

    return tiles;
}

bool BiomeClassifier::classifyFromClimate(
    const ClimateState& climateState,
    TileData* tiles,
    const u32 width,
    const u32 height) {
    if (!tiles || width == 0 || height == 0 || climateState.completedClimateYears == 0 ||
        !climateState.completedAnnualPrecipitation || !climateState.completedAnnualMeanTemperatureKelvin ||
        !climateState.completedColdestQuarterMeanTemperatureKelvin ||
        !climateState.completedWarmestQuarterMeanTemperatureKelvin ||
        !climateState.completedDriestQuarterPrecipitation ||
        !climateState.completedWettestQuarterPrecipitation) {
        return false;
    }

    bool anyChanged = false;
    const u32 total = width * height;
    for (u32 index = 0; index < total; ++index) {
        TileData& tile = tiles[index];
        if (isWaterBiome(tile.biome)) {
            continue;
        }

        const ClimateBiomeInputs climate = {
            climateState.completedAnnualMeanTemperatureKelvin[index] - KELVIN_OFFSET,
            climateState.completedColdestQuarterMeanTemperatureKelvin[index] - KELVIN_OFFSET,
            climateState.completedWarmestQuarterMeanTemperatureKelvin[index] - KELVIN_OFFSET,
            climateState.completedAnnualPrecipitation[index],
            climateState.completedDriestQuarterPrecipitation[index],
            climateState.completedWettestQuarterPrecipitation[index],
        };
        const BiomeType biome = classifyClimateLandBiome(tile.relief, climate);
        const FeatureFlags updatedFeatures = updateBiomeFeatures(tile.features, biome);

        if (tile.biome != biome || tile.features != updatedFeatures) {
            tile.biome = biome;
            tile.features = updatedFeatures;
            anyChanged = true;
        }
    }

    return anyChanged;
}