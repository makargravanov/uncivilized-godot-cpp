//
// Created by Alex on 24.06.2025.
//

#include "LayerSeparator.h"

#include <queue>
f32 LayerSeparator::findThreshold(std::unique_ptr<f32[]>& heightMap, f32 landPercentage, const u32 width, const u32 height) {


    // TODO:
    return 0;
}
void LayerSeparator::fillOcean(const std::unique_ptr<f32[]>& heights, std::unique_ptr<bool[]>& ocean,
    const u32 width, const u32 height, const f32 oceanLevel) {

    for (u32 i = 0; i < width * height; ++i) {
        ocean[i] = false;
    }

    std::queue<std::pair<u32, u32>> toExpand;

    for (u32 x = 0; x < width; ++x) {
        if (heights[x] <= oceanLevel) {
            toExpand.push({x, 0});
        }
        if (heights[(height - 1) * width + x] <= oceanLevel) {
            toExpand.push({x, height - 1});
        }
    }

    for (u32 y = 0; y < height; ++y) {
        if (heights[y * width] <= oceanLevel) {
            toExpand.push({0, y});
        }
        if (heights[y * width + (width - 1)] <= oceanLevel) {
            toExpand.push({width - 1, y});
        }
    }

    while (!toExpand.empty()) {
        auto [x, y] = toExpand.front();
        toExpand.pop();
        u32 idx = y * width + x;
        if (!ocean[idx]) {
            ocean[idx] = true;
            for (i32 dy = -1; dy <= 1; ++dy) {
                for (i32 dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    i32 nx = static_cast<i32>(x) + dx;
                    i32 ny = static_cast<i32>(y) + dy;
                    if (nx >= 0 && nx < static_cast<i32>(width) && ny >= 0 && ny < static_cast<i32>(height)) {
                        u32 nidx = ny * width + nx;
                        if (!ocean[nidx] && heights[nidx] <= oceanLevel) {
                            toExpand.push({static_cast<u32>(nx), static_cast<u32>(ny)});
                        }
                    }
                }
            }
        }
    }
}

SeparatedMapResult LayerSeparator::initializeOceanAndThresholds(MapResult&& map) noexcept {
    auto heights = std::move(map.heights);
    auto ocean   = std::make_unique<bool[]>(map.height * map.width);

    fillOcean(heights, ocean, map.width, map.height, map.oceanLevel);



    return;
}
