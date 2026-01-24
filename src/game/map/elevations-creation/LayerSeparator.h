//
// Created by Alex on 24.06.2025.
//

#ifndef LAYERSEPARATOR_H
#define LAYERSEPARATOR_H
#include "PlatecWrapper.h"
#include <algorithm>


enum DiscreteLandTypeByHeight : u8;
struct MapResult;

constexpr u32 RELIEF_WINDOW_RADIUS = 2;       // 1 → 3×3, 2 → 5×5, 3 → 7×7
constexpr f32 HEIGHT_WEIGHT = 0.3f;           // α: вес высоты (1-α → вес relief)
constexpr f32 HILL_PERCENTILE = 0.80f;
constexpr f32 MOUNTAIN_PERCENTILE = 0.96f;
constexpr f32 PLATEAU_RELIEF_THRESHOLD = 0.15f;  // ниже этого relief → плато

constexpr f32 COASTAL_BASE_DISTANCE = 4.0f;      // базовая ширина прибрежья в клетках
constexpr f32 COASTAL_VARIATION = 1.5f;          // +- вариация (итого ~2.5 - 5.5 клеток)
constexpr f32 COASTAL_NOISE_SCALE = 0.05f;       // масштаб шума (меньше = плавнее границы)
constexpr f32 SHALLOW_OCEAN_PERCENTILE = 0.05f;  // 5% океана по высоте → мелководье

struct SeparatedMapResult {
    MapResult mapResult;
    std::unique_ptr<DiscreteLandTypeByHeight[]> discrete;
    f32 oceanThreshold    = 0;
    f32 hillThreshold     = 0;
    f32 mountainThreshold = 0;

    SeparatedMapResult(MapResult mapResult, std::unique_ptr<DiscreteLandTypeByHeight[]> discrete,
        f32 oceanThreshold, f32 hillThreshold, f32 mountainThreshold)
        : mapResult(std::move(mapResult)), discrete(std::move(discrete)), oceanThreshold(oceanThreshold), hillThreshold(hillThreshold),
          mountainThreshold(mountainThreshold) {}

    SeparatedMapResult(const SeparatedMapResult& other) = delete;
    SeparatedMapResult& operator=(const SeparatedMapResult& other) = delete;

    SeparatedMapResult(SeparatedMapResult&& other) noexcept
        : mapResult(std::move(other.mapResult)), discrete(std::move(other.discrete)),
          oceanThreshold(other.oceanThreshold), hillThreshold(other.hillThreshold),
          mountainThreshold(other.mountainThreshold) {}

    SeparatedMapResult& operator=(SeparatedMapResult&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        mapResult         = std::move(other.mapResult);
        discrete          = std::move(other.discrete);

        oceanThreshold    = other.oceanThreshold;
        hillThreshold     = other.hillThreshold;
        mountainThreshold = other.mountainThreshold;
        return *this;
    }
};

class LayerSeparator {
public:
    static SeparatedMapResult initializeOceanAndThresholdsByGradient(MapResult&& map);
    static SeparatedMapResult initializeOceanAndThresholdsByGradient(MapResult&& map, f32 oceanLevelOverride);

    __forceinline static constexpr u32 wrapX(const i32 x, const u32 width) {
        const i32 w = static_cast<i32>(width);
        return static_cast<u32>((x % w + w) % w);
    }
    __forceinline static constexpr i32 clampY(const i32 y, const u32 height) {
        return std::clamp(y, 0, static_cast<i32>(height - 1));
    }
private:
    static f32 findThreshold(
        const std::unique_ptr<f32[]>& heightMap, f32 landPercentage,
        u32 width, u32 height);

    static void fillOceanOrValleyOrPlain(const std::unique_ptr<f32[]>& heights, std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height, f32 oceanLevel);

    static std::unique_ptr<f32[]> computeReliefMap(
        const std::unique_ptr<f32[]>& heights,
        const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height, u32 radius);

    static void normalizeMap(f32* map, u32 size);

    static std::unique_ptr<u32[]> computeDistanceFromLand(
        const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height);

    static void markCoastalShallows(
        std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        const std::unique_ptr<u32[]>& distanceMap,
        u32 width, u32 height,
        u64 seed);

    static void markHighWaterAsShallow(
        const std::unique_ptr<f32[]>& heights,
        std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height,
        f32 percentile);
};


#endif // LAYERSEPARATOR_H
