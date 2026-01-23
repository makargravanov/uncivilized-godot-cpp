//
// Created by Alex on 24.06.2025.
//

#ifndef LAYERSEPARATOR_H
#define LAYERSEPARATOR_H
#include "PlatecWrapper.h"


enum DiscreteLandTypeByHeight : u8;
struct MapResult;

constexpr u32 RELIEF_WINDOW_RADIUS = 2;       // 1 → 3×3, 2 → 5×5, 3 → 7×7
constexpr f32 HEIGHT_WEIGHT = 0.3f;           // α: вес высоты (1-α → вес relief)
constexpr f32 HILL_PERCENTILE = 0.90f;
constexpr f32 MOUNTAIN_PERCENTILE = 0.97f;
constexpr f32 PLATEAU_RELIEF_THRESHOLD = 0.15f;  // ниже этого relief → плато

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
    static SeparatedMapResult initializeOceanAndThresholds(MapResult&& map);
    static SeparatedMapResult initializeOceanAndThresholds(MapResult&& map, f32 oceanLevelOverride);

    static SeparatedMapResult initializeOceanAndThresholdsByGradient(MapResult&& map);
    static SeparatedMapResult initializeOceanAndThresholdsByGradient(MapResult&& map, f32 oceanLevelOverride);
private:
    static f32 findThreshold(
        const std::unique_ptr<f32[]>& heightMap, f32 landPercentage,
        u32 width, u32 height);

    static void fillOceanOrValleyOrPlain(const std::unique_ptr<f32[]>& heights, std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height, f32 oceanLevel);

    static std::unique_ptr<f32[]> computeReliefMap(
        const std::unique_ptr<f32[]>& heights,
        u32 width, u32 height, u32 radius);

    static void normalizeMap(f32* map, u32 size);
};


#endif // LAYERSEPARATOR_H
