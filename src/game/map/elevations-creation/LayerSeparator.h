//
// Created by Alex on 24.06.2025.
//

#ifndef LAYERSEPARATOR_H
#define LAYERSEPARATOR_H
#include "PlatecWrapper.h"


enum DiscreteLandTypeByHeight : u8;
struct MapResult;

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
    static SeparatedMapResult initializeOceanAndThresholds(MapResult&& map) noexcept;

private:
    static f32 findThreshold(
        const std::unique_ptr<f32[]>& heightMap, f32 landPercentage,
        const u32 width, const u32 height);

    static void fillOceanOrPlain(const std::unique_ptr<f32[]>& heights, std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
        u32 width, u32 height, f32 oceanLevel);
};


#endif // LAYERSEPARATOR_H
