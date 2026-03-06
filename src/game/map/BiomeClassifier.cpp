//
// Created by Copilot on 06.03.2026.
//

#include "BiomeClassifier.h"
#include "climate/ClimateModel.h"

#include <algorithm>

void BiomeClassifier::reclassifyFromClimate(TileData* tiles, const ClimateModel& climate, u32 total) {
    for (u32 i = 0; i < total; ++i) {
        if (isWaterBiome(tiles[i].biome)) continue; // skip ocean tiles

        // Use annual averages for stable biome classification
        // (not seasonal instantaneous values).
        const f32 temp_c  = climate.getAnnualTemperatureCelsius(static_cast<i32>(i));
        const f32 precip  = climate.getAnnualPrecipitation(static_cast<i32>(i));
        const f32 elev    = climate.getElevation(static_cast<i32>(i));

        tiles[i].biome = whittaker(temp_c, precip, elev, tiles[i].relief);

        // TileData stores instantaneous snapshot for overlays.
        const f32 inst_t = climate.getTemperatureCelsius(static_cast<i32>(i));
        tiles[i].temperature   = static_cast<i8>(std::clamp(inst_t, -128.0f, 127.0f));
        tiles[i].precipitation = static_cast<u16>(std::clamp(precip, 0.0f, 65535.0f));
    }
}

BiomeType BiomeClassifier::whittaker(f32 temp_c, f32 precip_mm, f32 elevation_m, ReliefType relief) {
    // High-altitude overrides.
    if (relief == RELIEF_HIGH_MOUNTAIN || elevation_m > 5000.0f) {
        return BIOME_ALPINE;
    }
    if (relief == RELIEF_MOUNTAIN && elevation_m > 3000.0f) {
        return (temp_c < -5.0f) ? BIOME_ALPINE : BIOME_HIGHLAND;
    }

    // Whittaker diagram: biome = f(temperature, annual precipitation in mm/year).
    if (temp_c < -10.0f) {
        return BIOME_ICE_SHEET;
    }
    if (temp_c < -5.0f) {
        return (precip_mm > 100.0f) ? BIOME_TUNDRA : BIOME_ICE_SHEET;
    }
    if (temp_c < 5.0f) {
        // Boreal zone.
        if (precip_mm > 300.0f) return BIOME_BOREAL_FOREST;
        if (precip_mm > 100.0f) return BIOME_TUNDRA;
        return BIOME_TUNDRA;
    }
    if (temp_c < 20.0f) {
        // Temperate zone.
        if (precip_mm > 600.0f) return BIOME_TEMPERATE_FOREST;
        if (precip_mm > 200.0f) return BIOME_TEMPERATE_GRASSLAND;
        return BIOME_TEMPERATE_DESERT;
    }
    // Tropical zone (T ≥ 20°C).
    if (precip_mm > 1500.0f) return BIOME_TROPICAL_RAINFOREST;
    if (precip_mm > 800.0f) return BIOME_TROPICAL_SEASONAL;
    if (precip_mm > 300.0f) return BIOME_SAVANNA;
    return BIOME_SUBTROPICAL_DESERT;
}
