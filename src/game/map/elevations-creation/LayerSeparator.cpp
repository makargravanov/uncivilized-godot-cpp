//
// Created by Alex on 24.06.2025.
//

#include "LayerSeparator.h"

#include "DiscreteLandTypeByHeight.h"
#include <algorithm>
#include <cmath>
#include <queue>

f32 quantile(std::vector<f32>& data, const f32 landPercentage) {
    std::sort(data.begin(), data.end());
    const f32 index    = landPercentage * (data.size() - 1);
    const f32 lower    = std::floor(index);
    const f32 fraction = index - lower;
    return data[lower] + fraction * (data[lower+1] - data[lower]);
}

f32 LayerSeparator::findThreshold(           // от 0.0 до 1.0
    const std::unique_ptr<f32[]>& heightMap, const f32 landPercentage, const u32 width, const u32 height) {

    auto vec = std::vector<f32>(width*height);
    for (int i = 0; i < width*height; ++i) {
        vec[i] = heightMap[i];
    }
    return quantile(vec, landPercentage);
}

void LayerSeparator::fillOceanOrPlain(const std::unique_ptr<f32[]>& heights, std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
    const u32 width, const u32 height, const f32 oceanLevel) {

    for (u32 i = 0; i < width * height; ++i) {
        discrete[i] = DiscreteLandTypeByHeight::PLAIN;
    }

    std::queue<std::pair<u32, u32>> toExpand;

    for (u32 x = 0; x < width; ++x) {
        if (heights[x] <= oceanLevel) {
            toExpand.push({x, 0});
            discrete[x] = DiscreteLandTypeByHeight::OCEAN;
        }
        if (heights[(height - 1) * width + x] <= oceanLevel) {
            toExpand.push({x, height - 1});
            discrete[(height - 1) * width + x] = DiscreteLandTypeByHeight::OCEAN;
        }
    }

    for (u32 y = 0; y < height; ++y) {
        if (heights[y * width] <= oceanLevel) {
            toExpand.push({0, y});
            discrete[y * width] = DiscreteLandTypeByHeight::OCEAN;
        }
        if (heights[y * width + (width - 1)] <= oceanLevel) {
            toExpand.push({width - 1, y});
            discrete[y * width + (width - 1)] = DiscreteLandTypeByHeight::OCEAN;
        }
    }

    while (!toExpand.empty()) {
        auto [x, y] = toExpand.front();
        toExpand.pop();

        for (i32 dy = -1; dy <= 1; ++dy) {
            for (i32 dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                i32 nx = static_cast<i32>(x) + dx;
                i32 ny = static_cast<i32>(y) + dy;
                if (nx >= 0 && nx < static_cast<i32>(width) && ny >= 0 && ny < static_cast<i32>(height)) {
                    u32 nidx = ny * width + nx;
                    if (discrete[nidx] == DiscreteLandTypeByHeight::PLAIN && heights[nidx] <= oceanLevel) {
                        discrete[nidx] = DiscreteLandTypeByHeight::OCEAN;
                        toExpand.push({static_cast<u32>(nx), static_cast<u32>(ny)});
                    }
                }
            }
        }
    }
}

SeparatedMapResult LayerSeparator::initializeOceanAndThresholds(MapResult&& map) noexcept {
    auto heights = std::move(map.heights);
    auto discrete   = std::make_unique<DiscreteLandTypeByHeight[]>(map.height * map.width);

    fillOceanOrPlain(heights, discrete, map.width, map.height, map.oceanLevel);

    f32 thresholdHill = findThreshold(heights, 0.95, map.width, map.height);
    f32 thresholdMountain = findThreshold(heights, 0.97, map.width, map.height);

    u32 size = map.width * map.height;

    for (int i = 0; i < size; ++i) {
        if (discrete[i] == PLAIN) {
            if (heights[i] >= thresholdHill && heights[i] < thresholdMountain) {
                discrete[i] = HILL;
            }else if (heights[i] >= thresholdMountain){
                discrete[i] = MOUNTAIN;
            }else {

            }
        }
    }

    auto mapResult = MapResult(
        std::move(heights),
        std::move(map.ageMap),
        std::move(map.platesMap),
        map.width,
        map.height,
        map.oceanLevel
    );

    return SeparatedMapResult(
        std::move(mapResult),
        std::move(discrete),
        map.oceanLevel,
        thresholdHill,
        thresholdMountain
    );
}
