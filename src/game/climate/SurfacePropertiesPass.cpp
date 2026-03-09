#include "SurfacePropertiesPass.h"

#include <algorithm>

#include "ClimateConfig.h"
#include "game/map/BiomeType.h"
#include "game/map/FeatureType.h"
#include "game/map/TileData.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

bool hasFeature(const FeatureFlags features, const FeatureFlags flag) {
    return (features & flag) != 0;
}

f32 getBaseBiomeAlbedo(const BiomeType biome) {
    switch (biome) {
        case BIOME_DEEP_OCEAN:
            return CONFIG.surface.deepWaterAlbedo;
        case BIOME_SHALLOW_OCEAN:
        case BIOME_INLAND_SEA:
        case BIOME_SHALLOW_INLAND_SEA:
            return CONFIG.surface.shallowWaterAlbedo;
        case BIOME_POLAR_DESERT:
            return 0.34f;
        case BIOME_TUNDRA:
            return 0.24f;
        case BIOME_BOREAL_FOREST:
            return 0.16f;
        case BIOME_COLD_STEPPE:
            return 0.23f;
        case BIOME_TEMPERATE_FOREST:
            return 0.17f;
        case BIOME_TEMPERATE_STEPPE:
            return 0.22f;
        case BIOME_XERIC_SHRUBLAND:
            return 0.27f;
        case BIOME_HOT_DESERT:
            return 0.33f;
        case BIOME_SAVANNA:
            return 0.20f;
        case BIOME_TROPICAL_SEASONAL_FOREST:
            return 0.15f;
        case BIOME_TROPICAL_RAINFOREST:
            return 0.13f;
        case BIOME_ALPINE:
            return 0.30f;
        case BIOME_TYPE_COUNT:
        default:
            return CONFIG.surface.landReferenceAlbedo;
    }
}

f32 getBaseHeatCapacity(const TileData& tile) {
    if (isWaterBiome(tile.biome)) {
        return tile.biome == BIOME_DEEP_OCEAN
            ? CONFIG.surface.deepWaterHeatCapacity
            : CONFIG.surface.shallowWaterHeatCapacity;
    }

    switch (tile.relief) {
        case RELIEF_HILL:
            return CONFIG.surface.hillHeatCapacity;
        case RELIEF_MOUNTAIN:
        case RELIEF_HIGH_MOUNTAIN:
            return CONFIG.surface.mountainHeatCapacity;
        case RELIEF_OCEAN:
            return CONFIG.surface.shallowWaterHeatCapacity;
        case RELIEF_LAND:
        default:
            return CONFIG.surface.landHeatCapacity;
    }
}

} // namespace

void SurfacePropertiesPass::initialize(ClimateState& climateState, const TileData* tiles) {
    refreshFromTiles(climateState, tiles);
}

void SurfacePropertiesPass::refreshFromTiles(ClimateState& climateState, const TileData* tiles) {
    if (!tiles || !climateState.forestCoverFraction || !climateState.surfaceAlbedo ||
        !climateState.effectiveHeatCapacity) {
        return;
    }

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const TileData& tile = tiles[index];
        const bool forestCapable = isForestCapableBiome(tile.biome);
        const bool hasForest = forestCapable && hasFeature(tile.features, Feature::HAS_FOREST);
        const bool wasCleared = forestCapable && hasFeature(tile.features, Feature::FOREST_CLEARED) && !hasForest;
        const bool hasIce = hasFeature(tile.features, Feature::HAS_ICE);

        const f32 forestCoverFraction = hasForest ? 1.0f : 0.0f;
        climateState.forestCoverFraction[index] = forestCoverFraction;

        f32 surfaceAlbedo = getBaseBiomeAlbedo(tile.biome);
        if (forestCoverFraction > 0.0f) {
            surfaceAlbedo +=
                (CONFIG.surface.forestCanopyAlbedo - surfaceAlbedo) * forestCoverFraction;
        }
        if (wasCleared && !isWaterBiome(tile.biome)) {
            surfaceAlbedo = std::min(surfaceAlbedo + CONFIG.surface.clearedLandAlbedoBoost, 1.0f);
        }
        if (hasIce) {
            surfaceAlbedo = std::max(surfaceAlbedo, CONFIG.surface.iceAlbedo);
        }
        climateState.surfaceAlbedo[index] = std::clamp(surfaceAlbedo, 0.0f, 1.0f);

        f32 effectiveHeatCapacity = getBaseHeatCapacity(tile);
        effectiveHeatCapacity += CONFIG.surface.forestHeatCapacityBonus * forestCoverFraction;
        if (hasIce) {
            effectiveHeatCapacity += CONFIG.surface.iceHeatCapacityBonus;
        }
        climateState.effectiveHeatCapacity[index] = std::max(effectiveHeatCapacity, 1e-3f);
    }
}