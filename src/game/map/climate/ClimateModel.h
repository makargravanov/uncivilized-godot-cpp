//
// Created by Copilot on 06.03.2026.
//

#ifndef CLIMATEMODEL_H
#define CLIMATEMODEL_H

#include "ClimateData.h"
#include "util/declarations.h"

#include <memory>

struct TileData;

// Energy-Balance climate model.
// Temperature via EBM (insolation + lapse rate + diffusion + thermal inertia).
// Parameterized wind field (Hadley/Ferrel/Polar cells, seasonal ITCZ shift).
// Moisture advection along wind with orographic forcing.
// Biome classification from Whittaker diagram (temperature × precipitation).
class ClimateModel {
public:
    // Construct with raw heightmap data from platec.
    // heights: raw fp heightmap, oceanLevel: threshold, width/height: grid dimensions.
    ClimateModel(const f32* heights, f32 oceanLevel, u32 width, u32 height);

    // Run N warmup iterations to reach near-equilibrium from initial conditions.
    void warmup(u32 iterations);

    // Advance climate by one tick (year_fraction, e.g. 1/48 for one game turn).
    void tick(f32 year_fraction);

    // Read-only access — instantaneous (current tick).
    f32 getTemperatureCelsius(i32 index) const;
    f32 getPrecipitation(i32 index) const;
    f32 getMoisture(i32 index) const;
    f32 getElevation(i32 index) const;
    const WindCell& getWind(i32 index) const;

    // Annual averages — stable values for biome classification.
    f32 getAnnualTemperatureCelsius(i32 index) const;
    f32 getAnnualPrecipitation(i32 index) const;

    // Write climate results back to TileData array.
    void writeToTiles(TileData* tiles, u32 count) const;

    // Compute diagnostic metrics (energy balance, T stats, ice fraction, etc.)
    Climate::Metrics computeMetrics() const;
    const Climate::Metrics& getLastMetrics() const { return lastMetrics; }

    u32 getWidth() const { return gridWidth; }
    u32 getHeight() const { return gridHeight; }

    // Current simulation time in years (accumulates via tick()).
    f32 getYearFraction() const { return currentYear; }

    // Global modifiers for events (nuclear winter, volcanic eruptions, etc.)
    void setAlbedoModifier(f32 mod) { albedoModifier = mod; }
    void setSolarModifier(f32 mod) { solarModifier = mod; }

private:
    // Initialization
    void initCells(const f32* heights, f32 oceanLevel);
    void initWind();

    // Per-tick substeps
    void computeInsolation(f32 year_fraction);
    void computeTemperature(f32 dt_years);
    void advectHeat(f32 dt_years);
    void diffuseHeat(f32 dt_years);
    void updateWind(f32 year_fraction);
    void computeEvaporation(f32 dt_years);
    void advectMoisture(f32 dt_years);
    void computePrecipitation(f32 dt_years);
    void diffuseMoisture(f32 dt_years);

    // Orbital mechanics (simplified)
    f32 solarDeclination(f32 year_fraction) const;
    f32 distanceFactor(f32 year_fraction) const;
    f32 dailyInsolation(f32 latitude, f32 declination, f32 dist_factor) const;

    // Helpers
    f32 saturationHumidity(f32 temp_kelvin) const;
    f32 getAlbedo(i32 index) const;
    i32 idx(i32 x, i32 y) const;            // wrapping in X, clamping in Y
    void heightGradient(i32 x, i32 y, f32& dhdx, f32& dhdy) const;

    // Grid
    u32 gridWidth  = 0;
    u32 gridHeight = 0;
    u32 totalCells = 0;

    // Climate state
    std::unique_ptr<ClimateCell[]> cells;
    std::unique_ptr<WindCell[]>    wind;
    std::unique_ptr<f32[]>         insolation; // W/m², per tile

    // Annual averaging ring buffers (48 ticks = 1 year).
    // Accumulators: summed each tick, finalized at year boundary.
    std::unique_ptr<f32[]> accumT;       // sum of temperature (K) over current year
    std::unique_ptr<f32[]> accumP;       // sum of precipitation (mm) over current year
    // Finalized annual values (updated every 48 ticks).
    std::unique_ptr<f32[]> meanAnnualT;  // mean temperature (K) over last full year
    std::unique_ptr<f32[]> totalAnnualP; // total precipitation (mm) over last full year
    u32 tickInYear = 0;                  // 0..47 counter within current year
    static constexpr u32 TICKS_PER_YEAR = 48;

    // Raw heightmap maximum (for normalization)
    f32 maxRawHeight = 1.0f;

    // Time tracking
    f32 currentYear = 0.0f;

    // Global event modifiers
    f32 albedoModifier = 0.0f;  // added to all albedo values
    f32 solarModifier  = 1.0f;  // multiplied to solar constant

    // Diagnostics
    Climate::Metrics lastMetrics;
    f32 prevGlobalMeanT = 0.0f;  // for dT/dt tracking
};

#endif //CLIMATEMODEL_H
