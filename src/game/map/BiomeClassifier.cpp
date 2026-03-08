#include "BiomeClassifier.h"

#include <algorithm>

#include "FeatureType.h"
#include "game/climate/ClimateConfig.h"
#include "game/climate/ClimateState.h"

namespace {

constexpr f32 KELVIN_OFFSET = ClimateSettings::DEFAULT_CLIMATE_CONFIG.shared.kelvinOffset;
constexpr f32 ALPINE_MEAN_TEMPERATURE_C = 1.0f;
constexpr f32 ALPINE_WARMEST_TURN_C = 10.0f;
constexpr f32 TUNDRA_MEAN_TEMPERATURE_C = -7.0f;
constexpr f32 TUNDRA_WARMEST_TURN_C = 8.0f;
constexpr f32 BOREAL_MEAN_TEMPERATURE_C = 4.0f;
constexpr f32 TROPICAL_MEAN_TEMPERATURE_C = 22.0f;
constexpr f32 DESERT_ANNUAL_PRECIPITATION = 0.22f;
constexpr f32 BOREAL_FOREST_ANNUAL_PRECIPITATION = 0.42f;
constexpr f32 TEMPERATE_FOREST_ANNUAL_PRECIPITATION = 0.68f;
constexpr f32 TROPICAL_FOREST_ANNUAL_PRECIPITATION = 0.95f;

bool isMountainRelief(const ReliefType relief) {
    return relief == RELIEF_MOUNTAIN || relief == RELIEF_HIGH_MOUNTAIN;
}

BiomeType classifyInitialBiome(const DiscreteLandTypeByHeight type) {
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
            return BIOME_GRASSLAND;
        case HILL:
            return BIOME_HIGHLAND;
        case MOUNTAIN:
        case HIGH_MOUNTAIN:
            return BIOME_ALPINE;
        default:
            return BIOME_GRASSLAND;
    }
}

BiomeType classifyClimateLandBiome(
    const ReliefType relief,
    const f32 annualMeanTemperatureC,
    const f32 annualMaxTemperatureC,
    const f32 annualPrecipitation) {
    if (isMountainRelief(relief)) {
        if (annualMeanTemperatureC <= ALPINE_MEAN_TEMPERATURE_C
            || annualMaxTemperatureC <= ALPINE_WARMEST_TURN_C) {
            return BIOME_ALPINE;
        }
        return BIOME_HIGHLAND;
    }

    if (annualMeanTemperatureC <= TUNDRA_MEAN_TEMPERATURE_C
        || annualMaxTemperatureC <= TUNDRA_WARMEST_TURN_C) {
        return BIOME_TUNDRA;
    }

    if (annualMeanTemperatureC <= BOREAL_MEAN_TEMPERATURE_C) {
        return annualPrecipitation >= BOREAL_FOREST_ANNUAL_PRECIPITATION
            ? BIOME_BOREAL_FOREST
            : BIOME_TUNDRA;
    }

    if (annualMeanTemperatureC >= TROPICAL_MEAN_TEMPERATURE_C) {
        if (annualPrecipitation < DESERT_ANNUAL_PRECIPITATION) {
            return BIOME_DESERT;
        }
        if (annualPrecipitation < TROPICAL_FOREST_ANNUAL_PRECIPITATION) {
            return BIOME_SAVANNA;
        }
        return BIOME_TROPICAL_FOREST;
    }

    if (annualPrecipitation < DESERT_ANNUAL_PRECIPITATION) {
        return BIOME_DESERT;
    }

    if (annualPrecipitation < TEMPERATE_FOREST_ANNUAL_PRECIPITATION) {
        return BIOME_GRASSLAND;
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
        const BiomeType biome = classifyInitialBiome(discrete[i]);
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
                tiles[i] = { RELIEF_LAND, BIOME_GRASSLAND };
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
        !climateState.completedAnnualTemperatureMaxKelvin) {
        return false;
    }

    bool anyChanged = false;
    const u32 total = width * height;
    for (u32 index = 0; index < total; ++index) {
        TileData& tile = tiles[index];
        if (isWaterBiome(tile.biome)) {
            continue;
        }

        const f32 annualMeanTemperatureC = climateState.completedAnnualMeanTemperatureKelvin[index] - KELVIN_OFFSET;
        const f32 annualMaxTemperatureC = climateState.completedAnnualTemperatureMaxKelvin[index] - KELVIN_OFFSET;
        const f32 annualPrecipitation = climateState.completedAnnualPrecipitation[index];
        const BiomeType biome = classifyClimateLandBiome(
            tile.relief,
            annualMeanTemperatureC,
            annualMaxTemperatureC,
            annualPrecipitation);
        const FeatureFlags updatedFeatures = updateBiomeFeatures(tile.features, biome);

        if (tile.biome != biome || tile.features != updatedFeatures) {
            tile.biome = biome;
            tile.features = updatedFeatures;
            anyChanged = true;
        }
    }

    return anyChanged;
}