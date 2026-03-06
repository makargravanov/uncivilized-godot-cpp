//
// Created by Copilot on 06.03.2026.
//

#ifndef CLIMATEDATA_H
#define CLIMATEDATA_H

#include "util/declarations.h"
#include <cmath>

// Per-tile working climate cell — higher precision than TileData fields.
// Used by ClimateModel during simulation; results are then packed into TileData.
struct ClimateCell {
    f32 elevation_m   = 0.0f;  // height in meters above sea level (0 for ocean)
    f32 latitude      = 0.0f;  // radians, from grid Y position
    f32 temperature   = 288.0f; // Kelvin (working precision, 288K ≈ 15°C)
    f32 moisture      = 0.0f;  // specific humidity (kg/kg), normalized 0–1 for simplicity
    f32 precipitation = 0.0f;  // mm/month accumulator
    bool is_ocean     = false;
};

// Per-tile wind vector (zonal + meridional).
struct WindCell {
    f32 u = 0.0f;  // zonal (east-west): positive = westerly (blowing east)
    f32 v = 0.0f;  // meridional (north-south): positive = northward

    f32 speed() const { return std::sqrt(u * u + v * v); }
};

// Constants for the climate model.
namespace Climate {

// Diagnostic snapshot — computed per-tick or on demand.
struct Metrics {
    // Energy balance (W/m², averaged over all cells)
    f32 absorbed_solar  = 0.0f;  // S·(1−α) mean incoming
    f32 emitted_ir      = 0.0f;  // ε·σ·T⁴ mean outgoing
    f32 toa_imbalance   = 0.0f;  // absorbed − emitted (>0 = warming)

    // Temperature (°C)
    f32 T_mean_global   = 0.0f;
    f32 T_mean_land     = 0.0f;
    f32 T_mean_ocean    = 0.0f;
    f32 T_min           = 0.0f;
    f32 T_max           = 0.0f;

    // Rate of change (°C per tick)
    f32 dT_mean         = 0.0f;  // change in global mean T since last tick

    // Surface fractions
    f32 ice_fraction    = 0.0f;  // fraction of cells below 0°C
    f32 snow_fraction   = 0.0f;  // fraction of land cells below −5°C

    // Moisture
    f32 moisture_mean   = 0.0f;
    f32 precip_mean_land = 0.0f; // mm/year, land only

    // Albedo
    f32 albedo_mean     = 0.0f;

    // Counts
    u32 land_cells      = 0;
    u32 ocean_cells     = 0;
};
    // Physics
    constexpr f32 SOLAR_CONSTANT   = 1361.0f;   // W/m² at 1 AU
    constexpr f32 STEFAN_BOLTZMANN = 5.67e-8f;   // W/(m²·K⁴)
    constexpr f32 EMISSIVITY       = 0.62f;      // effective emissivity (greenhouse effect)
    constexpr f32 LAPSE_RATE       = 0.005f;     // °C per meter (~5°C/km, reduced for game balance)
    constexpr f32 KELVIN_OFFSET    = 273.15f;

    // Albedo values
    constexpr f32 ALBEDO_OCEAN     = 0.06f;
    constexpr f32 ALBEDO_LAND      = 0.25f;
    constexpr f32 ALBEDO_FOREST    = 0.15f;
    constexpr f32 ALBEDO_DESERT    = 0.40f;
    constexpr f32 ALBEDO_ICE       = 0.55f;     // was 0.70 — too reflective for game
    constexpr f32 ALBEDO_SNOW      = 0.60f;     // was 0.80 — caused snowball feedback

    // Thermal inertia (τ in years — relax toward equilibrium)
    constexpr f32 TAU_OCEAN        = 0.25f;      // ~3 months
    constexpr f32 TAU_LAND         = 0.083f;     // ~1 month

    // Heat diffusion coefficients (tuned so equator-pole ΔT ≈ 50°C)
    constexpr f32 HEAT_DIFFUSION_LAND  = 2.5e-3f;
    constexpr f32 HEAT_DIFFUSION_OCEAN = 0.025f;  // 10x land: simulates ocean currents

    // Wind base speed (m/s)
    constexpr f32 WIND_BASE_SPEED  = 5.0f;

    // Orographic forcing
    constexpr f32 OROGRAPHIC_SCALE = 0.25f;

    // Moisture
    constexpr f32 EVAPORATION_COEFF  = 0.12f;     // ocean evaporation rate
    constexpr f32 PRECIP_ORO_COEFF   = 200.0f;    // orographic precipitation factor
    constexpr f32 PRECIP_CONV_COEFF  = 120.0f;    // convective precipitation factor
    constexpr f32 PRECIP_FRONT_COEFF = 40.0f;     // convergence/frontal precip
    constexpr f32 MOISTURE_DIFFUSION = 5e-3f;     // moisture smoothing

    // Orbital (Earth-like defaults)
    constexpr f32 OBLIQUITY_RAD    = 0.40928f;   // 23.44° in radians
    constexpr f32 ECCENTRICITY     = 0.0167f;
    constexpr f32 LON_PERIHELION   = 4.9382f;    // 283° in radians

    // Map scaling
    constexpr f32 METER_SCALE      = 8000.0f;    // max raw→meters so peak ≈ 8000m
}

#endif //CLIMATEDATA_H
