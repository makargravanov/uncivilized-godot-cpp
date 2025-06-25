//
// Created by Alex on 24.06.2025.
//

#ifndef LAYERSEPARATOR_H
#define LAYERSEPARATOR_H
#include "PlatecWrapper.h"

struct MapResult;

struct SeparatedMapResult {
    MapResult mapResult;
    std::unique_ptr<bool[]> ocean;
    f32 oceanThreshold    = 0;
    f32 hillThreshold     = 0;
    f32 mountainThreshold = 0;
};

class LayerSeparator {
    static f32 findThreshold(
        const std::unique_ptr<f32[]>& heightMap, f32 landPercentage,
        const u32 width, const u32 height);

    static void fillOcean(const std::unique_ptr<f32[]>& heights, std::unique_ptr<bool[]>& ocean,
        u32 width, u32 height, f32 oceanLevel);

public:
    static SeparatedMapResult initializeOceanAndThresholds(MapResult&& map) noexcept;
};


#endif // LAYERSEPARATOR_H
