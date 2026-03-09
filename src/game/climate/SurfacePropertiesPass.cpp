#include "SurfacePropertiesPass.h"

#include <algorithm>
#include <cstring>

#include "ClimateConfig.h"
#include "game/map/BiomeType.h"
#include "game/map/FeatureType.h"
#include "game/map/TileData.h"

namespace {

constexpr ClimateSettings::ClimateConfig CONFIG = ClimateSettings::DEFAULT_CLIMATE_CONFIG;

bool hasFeature(const FeatureFlags features, const FeatureFlags flag) {
    return (features & flag) != 0;
}

void setFeature(FeatureFlags& features, const FeatureFlags flag, const bool enabled) {
    if (enabled) {
        features |= flag;
    } else {
        features &= ~flag;
    }
}

bool isOceanTile(const ClimateState& climateState, const u32 index) {
    return climateState.relativeAltitude[index] < CONFIG.shared.oceanAltitudeThreshold;
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

f32 computeSnowfallFraction(const f32 temperatureCelsius) {
    const f32 transitionRange = std::max(CONFIG.surface.snowfallTransitionRangeC, 1e-3f);
    const f32 normalized =
        (CONFIG.surface.snowfallTemperatureC - temperatureCelsius + transitionRange)
        / (2.0f * transitionRange);
    return std::clamp(normalized, 0.0f, 1.0f);
}

void recomputeDynamicSurfaceProperties(ClimateState& climateState) {
    if (!climateState.baseSurfaceAlbedo || !climateState.baseHeatCapacity ||
        !climateState.forestCoverFraction || !climateState.snowCoverFraction ||
        !climateState.seaIceFraction || !climateState.surfaceAlbedo ||
        !climateState.effectiveHeatCapacity) {
        return;
    }

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f32 forestCoverFraction = std::clamp(climateState.forestCoverFraction[index], 0.0f, 1.0f);
        const f32 snowCoverFraction = std::clamp(climateState.snowCoverFraction[index], 0.0f, 1.0f);
        const f32 seaIceFraction = std::clamp(climateState.seaIceFraction[index], 0.0f, 1.0f);

        f32 surfaceAlbedo = climateState.baseSurfaceAlbedo[index];
        if (!isOceanTile(climateState, index) && forestCoverFraction > 0.0f) {
            surfaceAlbedo +=
                (CONFIG.surface.forestCanopyAlbedo - surfaceAlbedo) * forestCoverFraction;
        }

        if (isOceanTile(climateState, index)) {
            surfaceAlbedo += (CONFIG.surface.iceAlbedo - surfaceAlbedo) * seaIceFraction;
        } else {
            const f32 visibleSnowFraction = snowCoverFraction * std::clamp(
                1.0f - CONFIG.surface.canopySnowMaskStrength * forestCoverFraction,
                0.0f,
                1.0f);
            surfaceAlbedo += (CONFIG.surface.iceAlbedo - surfaceAlbedo) * visibleSnowFraction;
        }
        climateState.surfaceAlbedo[index] = std::clamp(surfaceAlbedo, 0.0f, 1.0f);

        f32 effectiveHeatCapacity = climateState.baseHeatCapacity[index];
        if (!isOceanTile(climateState, index)) {
            effectiveHeatCapacity += CONFIG.surface.forestHeatCapacityBonus * forestCoverFraction;
            effectiveHeatCapacity += CONFIG.surface.iceHeatCapacityBonus * snowCoverFraction;
        } else {
            effectiveHeatCapacity += (effectiveHeatCapacity * CONFIG.surface.seaIceHeatCapacityFactor
                    - effectiveHeatCapacity) * seaIceFraction;
        }
        climateState.effectiveHeatCapacity[index] = std::max(effectiveHeatCapacity, 1e-3f);
    }
}

void initializeCryosphereFromTemperature(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || !climateState.snowWaterEquivalent ||
        !climateState.snowCoverFraction || !climateState.seaIceFraction) {
        return;
    }

    std::memset(climateState.snowWaterEquivalent.get(), 0, climateState.tileCount * sizeof(f32));
    std::memset(climateState.snowCoverFraction.get(), 0, climateState.tileCount * sizeof(f32));
    std::memset(climateState.seaIceFraction.get(), 0, climateState.tileCount * sizeof(f32));

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f32 temperatureCelsius = climateState.temperatureKelvin[index] - CONFIG.shared.kelvinOffset;
        if (isOceanTile(climateState, index)) {
            if (temperatureCelsius < CONFIG.surface.seaIceFreezeTemperatureC) {
                climateState.seaIceFraction[index] = std::clamp(
                    (CONFIG.surface.seaIceFreezeTemperatureC - temperatureCelsius) / 8.0f,
                    0.0f,
                    1.0f);
            }
        } else {
            const f32 initialSnowCover = computeSnowfallFraction(temperatureCelsius);
            climateState.snowCoverFraction[index] = initialSnowCover;
            climateState.snowWaterEquivalent[index] =
                initialSnowCover * CONFIG.surface.snowFullCoverWaterEquivalent;
        }
    }
}

void updateCryosphereState(ClimateState& climateState) {
    if (!climateState.temperatureKelvin || !climateState.turnPrecipitation ||
        !climateState.snowWaterEquivalent || !climateState.snowCoverFraction ||
        !climateState.seaIceFraction) {
        return;
    }

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const f32 temperatureCelsius = climateState.temperatureKelvin[index] - CONFIG.shared.kelvinOffset;
        const f32 turnPrecipitation = std::max(climateState.turnPrecipitation[index], 0.0f);

        if (isOceanTile(climateState, index)) {
            climateState.snowWaterEquivalent[index] = 0.0f;
            climateState.snowCoverFraction[index] = 0.0f;

            f32 seaIceFraction = climateState.seaIceFraction[index];
            if (temperatureCelsius < CONFIG.surface.seaIceFreezeTemperatureC) {
                seaIceFraction +=
                    (CONFIG.surface.seaIceFreezeTemperatureC - temperatureCelsius)
                    * CONFIG.surface.seaIceGrowthRate;
            }
            if (temperatureCelsius > CONFIG.surface.seaIceMeltTemperatureC) {
                seaIceFraction -=
                    (temperatureCelsius - CONFIG.surface.seaIceMeltTemperatureC)
                    * CONFIG.surface.seaIceMeltRate;
            }
            climateState.seaIceFraction[index] = std::clamp(seaIceFraction, 0.0f, 1.0f);
            continue;
        }

        climateState.seaIceFraction[index] = 0.0f;

        const f32 snowfallFraction = computeSnowfallFraction(temperatureCelsius);
        const f32 snowfallAmount = turnPrecipitation * snowfallFraction;
        const f32 rainfallAmount = turnPrecipitation * (1.0f - snowfallFraction);

        f32 snowWaterEquivalent = climateState.snowWaterEquivalent[index] + snowfallAmount;
        if (temperatureCelsius > CONFIG.surface.snowMeltTemperatureC) {
            snowWaterEquivalent -=
                (temperatureCelsius - CONFIG.surface.snowMeltTemperatureC)
                * CONFIG.surface.snowMeltRate;
        }
        if (rainfallAmount > 0.0f && temperatureCelsius > CONFIG.surface.snowfallTemperatureC - 1.0f) {
            snowWaterEquivalent -= rainfallAmount * CONFIG.surface.rainOnSnowMeltFactor;
        }

        snowWaterEquivalent = std::max(snowWaterEquivalent, 0.0f);
        climateState.snowWaterEquivalent[index] = snowWaterEquivalent;
        climateState.snowCoverFraction[index] = std::clamp(
            snowWaterEquivalent / std::max(CONFIG.surface.snowFullCoverWaterEquivalent, 1e-4f),
            0.0f,
            1.0f);
    }
}

void refreshStaticSurfaceInputs(ClimateState& climateState, const TileData* tiles) {
    if (!tiles || !climateState.forestCoverFraction || !climateState.baseSurfaceAlbedo ||
        !climateState.baseHeatCapacity) {
        return;
    }

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const TileData& tile = tiles[index];
        const bool forestCapable = isForestCapableBiome(tile.biome);
        const bool hasForest = forestCapable && hasFeature(tile.features, Feature::HAS_FOREST);
        const bool wasCleared = forestCapable && hasFeature(tile.features, Feature::FOREST_CLEARED) && !hasForest;

        climateState.forestCoverFraction[index] = hasForest ? 1.0f : 0.0f;

        f32 baseSurfaceAlbedo = getBaseBiomeAlbedo(tile.biome);
        if (wasCleared && !isWaterBiome(tile.biome)) {
            baseSurfaceAlbedo = std::min(baseSurfaceAlbedo + CONFIG.surface.clearedLandAlbedoBoost, 1.0f);
        }
        climateState.baseSurfaceAlbedo[index] = baseSurfaceAlbedo;
        climateState.baseHeatCapacity[index] = getBaseHeatCapacity(tile);
    }
}

} // namespace

void SurfacePropertiesPass::initialize(ClimateState& climateState, const TileData* tiles) {
    refreshStaticSurfaceInputs(climateState, tiles);
    initializeCryosphereFromTemperature(climateState);
    recomputeDynamicSurfaceProperties(climateState);
}

void SurfacePropertiesPass::advanceOneTurn(ClimateState& climateState) {
    updateCryosphereState(climateState);
    recomputeDynamicSurfaceProperties(climateState);
}

void SurfacePropertiesPass::refreshFromTiles(ClimateState& climateState, const TileData* tiles) {
    refreshStaticSurfaceInputs(climateState, tiles);
    recomputeDynamicSurfaceProperties(climateState);
}

void SurfacePropertiesPass::publishToTiles(const ClimateState& climateState, TileData* tiles) {
    if (!tiles || !climateState.snowCoverFraction || !climateState.seaIceFraction) {
        return;
    }

    for (u32 index = 0; index < climateState.tileCount; ++index) {
        const bool hasIce = std::max(
                climateState.snowCoverFraction[index],
                climateState.seaIceFraction[index]) >= CONFIG.surface.iceFeatureCoverageThreshold;
        setFeature(tiles[index].features, Feature::HAS_ICE, hasIce);
    }
}