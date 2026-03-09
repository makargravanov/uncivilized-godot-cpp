#ifndef CLIMATECONFIG_H
#define CLIMATECONFIG_H

#include "util/declarations.h"

namespace ClimateSettings {

constexpr f32 DEG_TO_RAD = 0.01745329251994329577f;

constexpr f32 toRadians(const f32 degrees) {
    return degrees * DEG_TO_RAD;
}

struct ClimateSharedConfig {
    f32 kelvinOffset;
    f32 tileStepX;
    f32 tileStepZ;
    f32 minWindSpeedMps;
    f32 oceanAltitudeThreshold;
    f32 minHeightRange;
    f32 waterToDryAirMolecularRatio;
    f32 standardAtmosphericPressureKpa;
    f32 magnusCoefficientA;
    f32 magnusCoefficientB;
    f32 magnusBasePressureKpa;
};

struct ClimateTemperatureConfig {
    f32 minOverlayTemperatureC;
    f32 maxOverlayTemperatureC;
    f32 polarSeaLevelTemperatureC;
    f32 equatorSeaLevelTemperatureC;
    f32 maxAltitudeCoolingK;
    f32 turnRelaxationFactor;
    u32 latitudeSampleCount;
};

struct ClimateSurfaceConfig {
    f32 referenceAlbedo;
    f32 albedoTemperatureResponseK;
    f32 deepWaterAlbedo;
    f32 shallowWaterAlbedo;
    f32 landReferenceAlbedo;
    f32 deepWaterHeatCapacity;
    f32 shallowWaterHeatCapacity;
    f32 landHeatCapacity;
    f32 hillHeatCapacity;
    f32 mountainHeatCapacity;
    f32 forestCanopyAlbedo;
    f32 forestHeatCapacityBonus;
    f32 clearedLandAlbedoBoost;
    f32 iceAlbedo;
    f32 iceHeatCapacityBonus;
    f32 minTurnResponseFactor;
    f32 maxTurnResponseFactor;
};

struct ClimateWindConfig {
    f32 itczDeclinationTrackingFraction;
    f32 calmEquatorDegrees;
    f32 bandFadeDegrees;
    f32 tradeBandStartDegrees;
    f32 tradeBandEndDegrees;
    f32 westerlyBandStartDegrees;
    f32 westerlyBandEndDegrees;
    f32 polarBandStartDegrees;
    f32 polarBandEndDegrees;
    f32 tradeWindEastMps;
    f32 westerlyWindEastMps;
    f32 polarEasterlyWindEastMps;
    f32 tradeWindMeridionalMps;
    f32 westerlyWindMeridionalMps;
    f32 polarEasterlyWindMeridionalMps;
    f32 orographicDragFactor;
    f32 maxOrographicDrag;
    f32 orographicDeflectionFactor;
    f32 maxOrographicDeflection;
    f32 minGradientMagnitude;
    f32 equatorLatitudeEpsilonRadians;
};

struct ClimateMoistureConfig {
    f32 oceanEvaporationRate;
    f32 oceanInitialHumidityFraction;
    f32 landInitialHumidityFraction;
    u32 advectionSubSteps;
    f32 advectionMixingFraction;
    f32 referenceAdvectionWindSpeedMps;
    f32 maxAdvectionMixingFactor;
    f32 orographicLiftCoolingPerUnitUpslopeK;
    f32 maxOrographicLiftCoolingK;
    f32 orographicPrecipitationEfficiency;
    f32 oceanEvaporationWindFactor;
    f32 minOverlayHumidity;
    f32 maxOverlayHumidity;
    f32 minOverlayTurnPrecipitation;
    f32 maxOverlayTurnPrecipitation;
    f32 precipitationOverlayResponseExponent;
};

struct ClimateConfig {
    ClimateSharedConfig shared;
    ClimateTemperatureConfig temperature;
    ClimateSurfaceConfig surface;
    ClimateWindConfig wind;
    ClimateMoistureConfig moisture;
};

inline constexpr ClimateSharedConfig DEFAULT_SHARED_CONFIG = {
    273.15f,
    173.205078f / 100.0f,
    150.0f / 100.0f,
    1e-3f,
    1e-6f,
    1e-6f,
    0.622f,
    101.325f,
    17.27f,
    237.3f,
    0.6108f,
};

inline constexpr ClimateTemperatureConfig DEFAULT_TEMPERATURE_CONFIG = {
    -40.0f,
    40.0f,
    -22.0f,
    32.0f,
    38.0f,
    0.35f,
    19,
};

inline constexpr ClimateSurfaceConfig DEFAULT_SURFACE_CONFIG = {
    0.22f,
    10.0f,
    0.08f,
    0.12f,
    0.22f,
    3.2f,
    2.6f,
    1.0f,
    0.88f,
    0.72f,
    0.12f,
    0.22f,
    0.03f,
    0.55f,
    0.18f,
    0.05f,
    0.65f,
};

inline constexpr ClimateWindConfig DEFAULT_WIND_CONFIG = {
    0.35f,
    6.0f,
    7.0f,
    4.0f,
    30.0f,
    24.0f,
    60.0f,
    56.0f,
    86.0f,
    -8.0f,
    11.0f,
    -5.0f,
    3.5f,
    2.75f,
    1.75f,
    2.8f,
    0.7f,
    4.4f,
    0.6f,
    1e-4f,
    1e-4f,
};

inline constexpr ClimateMoistureConfig DEFAULT_MOISTURE_CONFIG = {
    0.015f,
    0.80f,
    0.20f,
    12,
    0.25f,
    8.0f,
    0.45f,
    18.0f,
    7.5f,
    0.65f,
    1.0f,
    0.0f,
    0.025f,
    0.0f,
    0.060f,
    0.5f,
};

inline constexpr ClimateConfig DEFAULT_CLIMATE_CONFIG = {
    DEFAULT_SHARED_CONFIG,
    DEFAULT_TEMPERATURE_CONFIG,
    DEFAULT_SURFACE_CONFIG,
    DEFAULT_WIND_CONFIG,
    DEFAULT_MOISTURE_CONFIG,
};

} // namespace ClimateSettings

#endif //CLIMATECONFIG_H
