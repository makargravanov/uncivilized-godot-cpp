//
// Created by Copilot on 06.03.2026.
//

#include "ClimateModel.h"
#include "game/map/TileData.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

static constexpr f32 PI = 3.14159265358979323846f;
static constexpr f32 TWO_PI = 2.0f * PI;

// ─── Construction & Init ─────────────────────────────────────────────────────

ClimateModel::ClimateModel(const f32* heights, f32 oceanLevel, u32 width, u32 height)
    : gridWidth(width), gridHeight(height), totalCells(width * height)
{
    cells      = std::make_unique<ClimateCell[]>(totalCells);
    wind       = std::make_unique<WindCell[]>(totalCells);
    insolation = std::make_unique<f32[]>(totalCells);

    // Annual averaging buffers.
    accumT       = std::make_unique<f32[]>(totalCells);
    accumP       = std::make_unique<f32[]>(totalCells);
    meanAnnualT  = std::make_unique<f32[]>(totalCells);
    totalAnnualP = std::make_unique<f32[]>(totalCells);
    for (u32 i = 0; i < totalCells; ++i) {
        accumT[i] = accumP[i] = meanAnnualT[i] = totalAnnualP[i] = 0.0f;
    }

    initCells(heights, oceanLevel);
    initWind();
}

void ClimateModel::initCells(const f32* heights, f32 oceanLevel) {
    // Find max land height for normalization to meters.
    maxRawHeight = 0.0f;
    for (u32 i = 0; i < totalCells; ++i) {
        if (heights[i] > oceanLevel) {
            maxRawHeight = std::max(maxRawHeight, heights[i] - oceanLevel);
        }
    }
    if (maxRawHeight < 1e-6f) maxRawHeight = 1.0f;

    for (u32 y = 0; y < gridHeight; ++y) {
        for (u32 x = 0; x < gridWidth; ++x) {
            const u32 i = y * gridWidth + x;
            auto& c = cells[i];

            // Latitude: top of map = +70°, bottom = -70° (in radians)
            c.latitude = (0.5f - static_cast<f32>(y) / static_cast<f32>(gridHeight)) * PI * 0.778f;

            // Elevation in meters above sea level (0 for ocean)
            if (heights[i] > oceanLevel) {
                c.elevation_m = ((heights[i] - oceanLevel) / maxRawHeight) * Climate::METER_SCALE;
                c.is_ocean = false;
            } else {
                c.elevation_m = 0.0f;
                c.is_ocean = true;
            }

            // Initial temperature: warm start to avoid ice-albedo trap.
            // Equator ~30°C, poles ~-20°C, then lapse rate correction.
            const f32 abs_lat = std::abs(c.latitude);
            c.temperature = 308.0f - 55.0f * (abs_lat / (PI * 0.389f))
                            - Climate::LAPSE_RATE * c.elevation_m * 0.5f;
            c.temperature = std::max(c.temperature, 220.0f);

            // Ocean tiles start with moisture, land starts dry.
            c.moisture = c.is_ocean ? 0.8f : 0.1f;
            c.precipitation = 0.0f;

            // Seed annual averages with initial temperature.
            meanAnnualT[i] = c.temperature;
            totalAnnualP[i] = 0.0f;
        }
    }
}

void ClimateModel::initWind() {
    updateWind(0.0f);
}

// ─── Warmup & Tick ───────────────────────────────────────────────────────────

void ClimateModel::warmup(u32 iterations) {
    constexpr f32 warmup_dt = 1.0f / 48.0f; // same as one game turn

    // Zero annual accumulators.
    for (u32 j = 0; j < totalCells; ++j) {
        accumT[j] = 0.0f;
        accumP[j] = 0.0f;
    }
    tickInYear = 0;

    for (u32 i = 0; i < iterations; ++i) {
        // Track mean T for dT computation.
        {
            f32 sum = 0.0f;
            for (u32 j = 0; j < totalCells; ++j) sum += cells[j].temperature;
            prevGlobalMeanT = sum / static_cast<f32>(totalCells);
        }

        // Reset per-tick precipitation before computing.
        for (u32 j = 0; j < totalCells; ++j) {
            cells[j].precipitation = 0.0f;
        }

        const f32 yearFrac = static_cast<f32>(tickInYear) / static_cast<f32>(TICKS_PER_YEAR);
        computeInsolation(yearFrac);
        computeTemperature(warmup_dt);
        diffuseHeat(warmup_dt);
        updateWind(yearFrac);
        computeEvaporation(warmup_dt);
        advectMoisture(warmup_dt);
        computePrecipitation(warmup_dt);
        diffuseMoisture(warmup_dt);

        // Accumulate into annual buffers (same logic as tick()).
        for (u32 j = 0; j < totalCells; ++j) {
            accumT[j] += cells[j].temperature;
            accumP[j] += cells[j].precipitation;
        }
        ++tickInYear;

        // Finalize annual averages at year boundary.
        if (tickInYear >= TICKS_PER_YEAR) {
            const f32 inv = 1.0f / static_cast<f32>(TICKS_PER_YEAR);
            for (u32 j = 0; j < totalCells; ++j) {
                meanAnnualT[j]  = accumT[j] * inv;
                totalAnnualP[j] = accumP[j];
                accumT[j] = 0.0f;
                accumP[j] = 0.0f;
            }
            tickInYear = 0;
        }

        // Log metrics every simulated year.
        if ((i + 1) % 48 == 0) {
            lastMetrics = computeMetrics();
            const auto& m = lastMetrics;
            std::printf("[Climate Y%u] T=%.1f°C (land=%.1f ocean=%.1f) "
                        "TOA=%.1f W/m² α=%.3f ice=%.0f%% snow=%.0f%% "
                        "precip=%.0f mm/yr dT=%.3f°C/tick\n",
                        (i + 1) / 48,
                        m.T_mean_global, m.T_mean_land, m.T_mean_ocean,
                        m.toa_imbalance, m.albedo_mean,
                        m.ice_fraction * 100.0f, m.snow_fraction * 100.0f,
                        m.precip_mean_land, m.dT_mean);
            std::fflush(stdout);
        }
    }
    currentYear = 0.0f;
}

void ClimateModel::tick(f32 year_fraction) {
    currentYear += year_fraction;
    const f32 yearFrac = static_cast<f32>(tickInYear) / static_cast<f32>(TICKS_PER_YEAR);

    // Track mean T for dT.
    {
        f32 sum = 0.0f;
        for (u32 i = 0; i < totalCells; ++i) sum += cells[i].temperature;
        prevGlobalMeanT = sum / static_cast<f32>(totalCells);
    }

    // Precipitation accumulates per-tick; we do NOT reset it here —
    // it's accumulated into annualP and reset at year boundary.
    for (u32 i = 0; i < totalCells; ++i) {
        cells[i].precipitation = 0.0f;
    }

    computeInsolation(yearFrac);
    computeTemperature(year_fraction);
    diffuseHeat(year_fraction);
    updateWind(yearFrac);
    computeEvaporation(year_fraction);
    advectMoisture(year_fraction);
    computePrecipitation(year_fraction);
    diffuseMoisture(year_fraction);

    // Accumulate this tick into annual buffers.
    for (u32 i = 0; i < totalCells; ++i) {
        accumT[i] += cells[i].temperature;
        accumP[i] += cells[i].precipitation;
    }
    ++tickInYear;

    // Finalize annual averages at year boundary.
    if (tickInYear >= TICKS_PER_YEAR) {
        const f32 inv = 1.0f / static_cast<f32>(TICKS_PER_YEAR);
        for (u32 j = 0; j < totalCells; ++j) {
            meanAnnualT[j]  = accumT[j] * inv;
            totalAnnualP[j] = accumP[j];
            accumT[j] = 0.0f;
            accumP[j] = 0.0f;
        }
        tickInYear = 0;
    }

    lastMetrics = computeMetrics();
}

// ─── Orbital / Insolation ────────────────────────────────────────────────────

f32 ClimateModel::solarDeclination(f32 year_fraction) const {
    // Simplified: δ = obliquity × sin(2π × year_fraction)
    // More precise: use longitude of perihelion offset.
    const f32 lambda = TWO_PI * year_fraction + Climate::LON_PERIHELION;
    return std::asin(std::sin(Climate::OBLIQUITY_RAD) * std::sin(lambda));
}

f32 ClimateModel::distanceFactor(f32 year_fraction) const {
    // (a/r)² from Kepler, simplified for small eccentricity.
    const f32 nu = TWO_PI * year_fraction; // approximate true anomaly ≈ mean anomaly
    const f32 e = Climate::ECCENTRICITY;
    const f32 r_inv = (1.0f + e * std::cos(nu)) / (1.0f - e * e);
    return r_inv * r_inv;
}

f32 ClimateModel::dailyInsolation(f32 latitude, f32 declination, f32 dist_factor) const {
    // Sunrise hour angle h₀.
    const f32 x = -std::tan(latitude) * std::tan(declination);
    f32 h0;
    if (x >= 1.0f) {
        h0 = 0.0f;         // polar night
    } else if (x <= -1.0f) {
        h0 = PI;            // polar day (midnight sun)
    } else {
        h0 = std::acos(x);
    }

    // Average cosine of zenith over the day.
    const f32 avg_cos_z = (1.0f / PI) * (
        h0 * std::sin(latitude) * std::sin(declination)
        + std::cos(latitude) * std::cos(declination) * std::sin(h0)
    );

    return Climate::SOLAR_CONSTANT * solarModifier * dist_factor * std::max(0.0f, avg_cos_z);
}

void ClimateModel::computeInsolation(f32 year_fraction) {
    const f32 decl = solarDeclination(year_fraction);
    const f32 df   = distanceFactor(year_fraction);

    for (u32 i = 0; i < totalCells; ++i) {
        insolation[i] = dailyInsolation(cells[i].latitude, decl, df);
    }
}

// ─── Temperature ─────────────────────────────────────────────────────────────

f32 ClimateModel::getAlbedo(i32 index) const {
    const auto& c = cells[index];
    f32 a;
    if (c.is_ocean) {
        // Smooth ice-albedo transition over 10K range (−7°C .. +3°C)
        const f32 t = std::clamp(
            (c.temperature - (Climate::KELVIN_OFFSET - 7.0f)) / 10.0f, 0.0f, 1.0f);
        a = Climate::ALBEDO_ICE * (1.0f - t) + Climate::ALBEDO_OCEAN * t;
    } else {
        // Smooth snow-albedo transition over 30K range (−25°C .. +5°C)
        // Wide range prevents ice-albedo bistability on temperate land.
        const f32 snow_t = std::clamp(
            (c.temperature - (Climate::KELVIN_OFFSET - 25.0f)) / 30.0f, 0.0f, 1.0f);
        f32 base;
        if (c.elevation_m > 4000.0f) {
            base = Climate::ALBEDO_SNOW * 0.5f + Climate::ALBEDO_LAND * 0.5f;
        } else if (c.moisture < 0.1f) {
            base = Climate::ALBEDO_DESERT;
        } else if (c.moisture > 0.4f) {
            base = Climate::ALBEDO_FOREST;
        } else {
            base = Climate::ALBEDO_LAND;
        }
        a = Climate::ALBEDO_SNOW * (1.0f - snow_t) + base * snow_t;
    }
    return std::clamp(a + albedoModifier, 0.0f, 0.95f);
}

void ClimateModel::computeTemperature(f32 dt_years) {
    for (u32 i = 0; i < totalCells; ++i) {
        auto& c = cells[i];

        const f32 albedo = getAlbedo(static_cast<i32>(i));

        // Equilibrium temperature from Stefan-Boltzmann:
        // T_eq = ( I * (1-α) / (ε·σ) )^(1/4)
        // dailyInsolation() already gives per-tile average, so no /4 geometric factor.
        const f32 absorbed = insolation[i] * (1.0f - albedo);
        const f32 T_eq_base = std::pow(
            std::max(0.0f, absorbed) / (Climate::EMISSIVITY * Climate::STEFAN_BOLTZMANN),
            0.25f
        );

        // Lapse rate correction for elevation (damped: full lapse rate
        // makes mountain interiors too cold for the ice-albedo balance).
        const f32 T_eq = T_eq_base - Climate::LAPSE_RATE * c.elevation_m * 0.7f;

        // Thermal inertia: relax toward equilibrium.
        const f32 tau = c.is_ocean ? Climate::TAU_OCEAN : Climate::TAU_LAND;
        const f32 relaxation = std::min(1.0f, dt_years / tau);
        c.temperature += (T_eq - c.temperature) * relaxation;

        // Clamp to reasonable range.
        c.temperature = std::clamp(c.temperature, 180.0f, 340.0f);
    }
}

void ClimateModel::diffuseHeat(f32 dt_years) {
    // Sub-stepped Laplacian diffusion with large coefficients.
    // This parameterises both atmospheric circulation and ocean currents
    // that redistribute heat from equator to poles.
    //
    // We use multiple sub-steps so we can have a large effective diffusion
    // without violating the CFL stability limit (D < 0.25 for 2D Laplacian).
    constexpr i32 SUB_STEPS = 8;
    constexpr f32 D_OCEAN = 0.22f;   // near stability limit → strong ocean currents
    constexpr f32 D_LAND  = 0.14f;   // atmosphere over land (increased: wind transport)

    auto temp_copy = std::make_unique<f32[]>(totalCells);

    for (i32 s = 0; s < SUB_STEPS; ++s) {
        for (u32 i = 0; i < totalCells; ++i) {
            temp_copy[i] = cells[i].temperature;
        }

        for (u32 y = 0; y < gridHeight; ++y) {
            for (u32 x = 0; x < gridWidth; ++x) {
                const i32 i = idx(x, y);
                const f32 T_center = temp_copy[i];

                const f32 T_left  = temp_copy[idx(static_cast<i32>(x) - 1, y)];
                const f32 T_right = temp_copy[idx(static_cast<i32>(x) + 1, y)];
                const f32 T_up    = temp_copy[idx(x, static_cast<i32>(y) - 1)];
                const f32 T_down  = temp_copy[idx(x, static_cast<i32>(y) + 1)];

                const f32 laplacian = (T_left + T_right + T_up + T_down) - 4.0f * T_center;
                const f32 D = cells[i].is_ocean ? D_OCEAN : D_LAND;
                cells[i].temperature += D * laplacian;
            }
        }
    }
}

// ─── Wind Field ──────────────────────────────────────────────────────────────

void ClimateModel::updateWind(f32 year_fraction) {
    // ITCZ latitude shifts with season: peaks at ±15° for the given hemisphere's summer.
    const f32 itcz_lat_rad = (15.0f * PI / 180.0f) * std::sin(TWO_PI * year_fraction);

    for (u32 y = 0; y < gridHeight; ++y) {
        for (u32 x = 0; x < gridWidth; ++x) {
            const i32 i = idx(x, y);
            const f32 lat = cells[i].latitude; // radians
            const f32 lat_deg = lat * 180.0f / PI;
            const f32 shifted_lat = lat - itcz_lat_rad; // latitude relative to ITCZ
            const f32 abs_shifted = std::abs(shifted_lat);
            const f32 abs_shifted_deg = abs_shifted * 180.0f / PI;
            const f32 sign = (shifted_lat >= 0.0f) ? 1.0f : -1.0f; // hemisphere sign

            f32 u_wind = 0.0f; // zonal
            f32 v_wind = 0.0f; // meridional

            if (abs_shifted_deg < 30.0f) {
                // Hadley cell: trade winds (easterly = negative u).
                // Meridional: toward ITCZ (equatorward).
                const f32 t = abs_shifted_deg / 30.0f;
                u_wind = -Climate::WIND_BASE_SPEED * (0.5f + 0.5f * t);
                v_wind = -sign * Climate::WIND_BASE_SPEED * 0.3f * (1.0f - t);
            } else if (abs_shifted_deg < 60.0f) {
                // Ferrel cell: westerlies (positive u).
                // Meridional: toward pole (poleward).
                const f32 t = (abs_shifted_deg - 30.0f) / 30.0f;
                u_wind = Climate::WIND_BASE_SPEED * (0.8f + 0.4f * std::sin(PI * t));
                v_wind = sign * Climate::WIND_BASE_SPEED * 0.2f * std::sin(PI * t);
            } else {
                // Polar cell: easterlies.
                // Meridional: toward equator.
                const f32 t = (abs_shifted_deg - 60.0f) / 30.0f;
                u_wind = -Climate::WIND_BASE_SPEED * 0.4f * (1.0f - t);
                v_wind = -sign * Climate::WIND_BASE_SPEED * 0.15f * (1.0f - t);
            }

            // Orographic deflection: reduce wind component pointing uphill.
            f32 dhdx, dhdy;
            heightGradient(x, y, dhdx, dhdy);

            // Dot product of wind and height gradient → uplift.
            const f32 dot = u_wind * dhdx + v_wind * dhdy;
            if (dot > 0.0f) {
                const f32 reduction = std::min(1.0f, dot * Climate::OROGRAPHIC_SCALE);
                u_wind *= (1.0f - reduction);
                v_wind *= (1.0f - reduction);
            }

            wind[i].u = u_wind;
            wind[i].v = v_wind;
        }
    }
}

void ClimateModel::heightGradient(i32 x, i32 y, f32& dhdx, f32& dhdy) const {
    // Central differences on elevation_m, wrapping X.
    const f32 h_left  = cells[idx(x - 1, y)].elevation_m;
    const f32 h_right = cells[idx(x + 1, y)].elevation_m;
    const f32 h_up    = cells[idx(x, y - 1)].elevation_m;
    const f32 h_down  = cells[idx(x, y + 1)].elevation_m;

    dhdx = (h_right - h_left) * 0.5f;
    dhdy = (h_down  - h_up)   * 0.5f;
}

// ─── Moisture & Precipitation ────────────────────────────────────────────────

f32 ClimateModel::saturationHumidity(f32 temp_kelvin) const {
    // Simplified Magnus formula: q_sat = f(T).
    // Returns normalized 0–1 value for our simplified moisture field.
    const f32 T_c = temp_kelvin - Climate::KELVIN_OFFSET;
    // Saturated vapor pressure (kPa).
    const f32 e_s = 0.6108f * std::exp(17.27f * T_c / (T_c + 237.3f));
    // Normalize: at 30°C, e_s ≈ 4.24 kPa. Map to ~1.0 at 35°C.
    return std::clamp(e_s / 5.5f, 0.0f, 1.0f);
}

void ClimateModel::computeEvaporation(f32 dt_years) {
    const f32 dt_scaled = dt_years * 48.0f;

    for (u32 i = 0; i < totalCells; ++i) {
        auto& c = cells[i];

        const f32 q_sat = saturationHumidity(c.temperature);
        const f32 deficit = std::max(0.0f, q_sat - c.moisture);
        const f32 ws = wind[i].speed();

        if (c.is_ocean) {
            // Ocean: strong evaporation.
            const f32 evap = Climate::EVAPORATION_COEFF * deficit
                           * (0.5f + ws / 10.0f) * dt_scaled;
            c.moisture = std::min(1.0f, c.moisture + evap);
        } else {
            // Land: weak evapotranspiration (recycles some soil moisture).
            // Scales with existing moisture (wetter land evaporates more).
            const f32 evap = Climate::EVAPORATION_COEFF * 0.15f
                           * deficit * c.moisture * dt_scaled;
            c.moisture = std::min(1.0f, c.moisture + evap);
        }
    }
}

void ClimateModel::advectMoisture(f32 dt_years) {
    // Upwind advection: q_new = q - dt * (u * dq/dx + v * dq/dy)
    auto moisture_copy = std::make_unique<f32[]>(totalCells);
    for (u32 i = 0; i < totalCells; ++i) {
        moisture_copy[i] = cells[i].moisture;
    }

    // CFL-limited effective dt: ensure stability.
    const f32 dt_scaled = std::min(0.8f, dt_years * 48.0f);

    for (u32 y = 0; y < gridHeight; ++y) {
        for (u32 x = 0; x < gridWidth; ++x) {
            const i32 i = idx(x, y);
            const f32 u_w = wind[i].u;
            const f32 v_w = wind[i].v;

            // Upwind differences.
            f32 dqdx, dqdy;
            if (u_w > 0.0f) {
                dqdx = moisture_copy[i] - moisture_copy[idx(static_cast<i32>(x) - 1, y)];
            } else {
                dqdx = moisture_copy[idx(static_cast<i32>(x) + 1, y)] - moisture_copy[i];
            }
            if (v_w > 0.0f) {
                dqdy = moisture_copy[i] - moisture_copy[idx(x, static_cast<i32>(y) - 1)];
            } else {
                dqdy = moisture_copy[idx(x, static_cast<i32>(y) + 1)] - moisture_copy[i];
            }

            // Advection step: normalize wind to cell-units/tick.
            // At 5 m/s → 1.0 cells/tick. Over 192 warmup ticks: ~192 cells.
            // CFL: max 10*0.2*0.8 = 1.6 — upwind scheme tolerates up to ~1-2.
            constexpr f32 WIND_TO_CELLS = 0.2f;
            const f32 u_cells = u_w * WIND_TO_CELLS;
            const f32 v_cells = v_w * WIND_TO_CELLS;

            cells[i].moisture = std::clamp(
                moisture_copy[i] - dt_scaled * (u_cells * dqdx + v_cells * dqdy),
                0.0f, 1.0f
            );
        }
    }
}

void ClimateModel::computePrecipitation(f32 dt_years) {
    const f32 dt_scaled = dt_years * 48.0f;

    for (u32 y = 0; y < gridHeight; ++y) {
        for (u32 x = 0; x < gridWidth; ++x) {
            const i32 i = idx(x, y);
            auto& c = cells[i];

            f32 precip = 0.0f;

            // 1. Orographic precipitation: forced ascent of moist air over mountains.
            f32 dhdx, dhdy;
            heightGradient(x, y, dhdx, dhdy);
            const f32 uplift = std::max(0.0f,
                wind[i].u * dhdx + wind[i].v * dhdy) * Climate::OROGRAPHIC_SCALE;
            precip += c.moisture * uplift * Climate::PRECIP_ORO_COEFF * dt_scaled;

            // 2. Convective precipitation: when moisture exceeds saturation.
            const f32 q_sat = saturationHumidity(c.temperature);
            if (c.moisture > q_sat) {
                const f32 excess = c.moisture - ;
                precip += excess * Climate::PRECIP_CONV_COEFF * dt_scaled;
            }

            // 3. Convergence / frontal precipitation.
            // Compute wind divergence: div(V) = du/dx + dv/dy.
            // Negative divergence = convergence = uplift = rain.
            const f32 u_left  = wind[idx(static_cast<i32>(x) - 1, y)].u;
            const f32 u_right = wind[idx(static_cast<i32>(x) + 1, y)].u;
            const f32 v_up    = wind[idx(x, static_cast<i32>(y) - 1)].v;
            const f32 v_down  = wind[idx(x, static_cast<i32>(y) + 1)].v;
            const f32 divergence = (u_right - u_left) * 0.5f + (v_down - v_up) * 0.5f;
            if (divergence < 0.0f) {
                // Convergence: stronger convergence + more moisture = more rain.
                precip += c.moisture * (-divergence) * Climate::PRECIP_FRONT_COEFF * dt_scaled;
            }

            // Remove precipitated moisture from atmosphere.
            // precip is in mm; roughly 30mm removes 0.01 of moisture.
            const f32 moisture_removed = std::min(c.moisture, precip / 3000.0f);
            c.moisture -= moisture_removed;
            c.moisture = std::max(0.0f, c.moisture);

            if (!c.is_ocean) {
                c.precipitation += precip; // mm/month accumulation
            }
        }
    }
}

void ClimateModel::diffuseMoisture(f32 dt_years) {
    auto moisture_copy = std::make_unique<f32[]>(totalCells);
    for (u32 i = 0; i < totalCells; ++i) {
        moisture_copy[i] = cells[i].moisture;
    }

    const f32 coeff = Climate::MOISTURE_DIFFUSION * dt_years * 48.0f;

    for (u32 y = 0; y < gridHeight; ++y) {
        for (u32 x = 0; x < gridWidth; ++x) {
            const i32 i = idx(x, y);
            const f32 q_center = moisture_copy[i];
            const f32 q_left  = moisture_copy[idx(static_cast<i32>(x) - 1, y)];
            const f32 q_right = moisture_copy[idx(static_cast<i32>(x) + 1, y)];
            const f32 q_up    = moisture_copy[idx(x, static_cast<i32>(y) - 1)];
            const f32 q_down  = moisture_copy[idx(x, static_cast<i32>(y) + 1)];

            const f32 laplacian = (q_left + q_right + q_up + q_down) - 4.0f * q_center;
            cells[i].moisture = std::clamp(
                cells[i].moisture + coeff * laplacian, 0.0f, 1.0f);
        }
    }
}

// ─── Output ──────────────────────────────────────────────────────────────────

f32 ClimateModel::getTemperatureCelsius(i32 index) const {
    return cells[index].temperature - Climate::KELVIN_OFFSET;
}

f32 ClimateModel::getPrecipitation(i32 index) const {
    return cells[index].precipitation;
}

f32 ClimateModel::getAnnualTemperatureCelsius(i32 index) const {
    return meanAnnualT[index] - Climate::KELVIN_OFFSET;
}

f32 ClimateModel::getAnnualPrecipitation(i32 index) const {
    return totalAnnualP[index];
}

f32 ClimateModel::getMoisture(i32 index) const {
    return cells[index].moisture;
}

f32 ClimateModel::getElevation(i32 index) const {
    return cells[index].elevation_m;
}

const WindCell& ClimateModel::getWind(i32 index) const {
    return wind[index];
}

void ClimateModel::writeToTiles(TileData* tiles, u32 count) const {
    for (u32 i = 0; i < count && i < totalCells; ++i) {
        const f32 temp_c = cells[i].temperature - Climate::KELVIN_OFFSET;
        tiles[i].temperature = static_cast<i8>(std::clamp(temp_c, -128.0f, 127.0f));
        tiles[i].precipitation = static_cast<u16>(std::clamp(cells[i].precipitation, 0.0f, 65535.0f));
    }
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

Climate::Metrics ClimateModel::computeMetrics() const {
    Climate::Metrics m{};

    f32 T_sum_global = 0.0f;
    f32 T_sum_land   = 0.0f;
    f32 T_sum_ocean  = 0.0f;
    f32 absorbed_sum = 0.0f;
    f32 emitted_sum  = 0.0f;
    f32 albedo_sum   = 0.0f;
    f32 moisture_sum = 0.0f;
    f32 precip_sum   = 0.0f;
    u32 ice_count    = 0;
    u32 snow_count   = 0;
    u32 land_count   = 0;
    u32 ocean_count  = 0;

    f32 T_min_k = 9999.0f;
    f32 T_max_k = 0.0f;

    for (u32 i = 0; i < totalCells; ++i) {
        const auto& c = cells[i];
        const f32 T = c.temperature;
        const f32 T_c = T - Climate::KELVIN_OFFSET;

        T_sum_global += T;
        if (T < T_min_k) T_min_k = T;
        if (T > T_max_k) T_max_k = T;

        if (T_c < 0.0f) ++ice_count;

        const f32 a = getAlbedo(static_cast<i32>(i));
        albedo_sum += a;
        absorbed_sum += insolation[i] * (1.0f - a);
        emitted_sum += Climate::EMISSIVITY * Climate::STEFAN_BOLTZMANN * T * T * T * T;
        moisture_sum += c.moisture;

        if (c.is_ocean) {
            T_sum_ocean += T;
            ++ocean_count;
        } else {
            T_sum_land += T;
            precip_sum += totalAnnualP[i]; // annual precipitation from last full year
            ++land_count;
            if (T_c < -5.0f) ++snow_count;
        }
    }

    const f32 n = static_cast<f32>(totalCells);
    m.absorbed_solar   = absorbed_sum / n;
    m.emitted_ir       = emitted_sum / n;
    m.toa_imbalance    = m.absorbed_solar - m.emitted_ir;

    m.T_mean_global    = T_sum_global / n - Climate::KELVIN_OFFSET;
    m.T_mean_land      = land_count  > 0 ? (T_sum_land  / land_count  - Climate::KELVIN_OFFSET) : 0.0f;
    m.T_mean_ocean     = ocean_count > 0 ? (T_sum_ocean / ocean_count - Climate::KELVIN_OFFSET) : 0.0f;
    m.T_min            = T_min_k - Climate::KELVIN_OFFSET;
    m.T_max            = T_max_k - Climate::KELVIN_OFFSET;
    m.dT_mean          = m.T_mean_global - (prevGlobalMeanT - Climate::KELVIN_OFFSET);

    m.ice_fraction     = static_cast<f32>(ice_count)  / n;
    m.snow_fraction    = land_count > 0 ? static_cast<f32>(snow_count) / land_count : 0.0f;

    m.moisture_mean    = moisture_sum / n;
    m.precip_mean_land = land_count > 0 ? precip_sum / land_count : 0.0f;
    m.albedo_mean      = albedo_sum / n;

    m.land_cells       = land_count;
    m.ocean_cells      = ocean_count;

    return m;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

i32 ClimateModel::idx(i32 x, i32 y) const {
    // Wrap X (toroidal), clamp Y (poles).
    const i32 w = static_cast<i32>(gridWidth);
    const i32 h = static_cast<i32>(gridHeight);
    x = ((x % w) + w) % w;
    y = std::clamp(y, 0, h - 1);
    return y * w + x;
}
