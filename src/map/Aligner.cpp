//
// Created by Alex on 11.06.2025.
//

#include "Aligner.h"

#include "PlatecWrapper.h"
#include <algorithm>
#include <vector>

MapResult Aligner::applyAlign(MapResult&& map) noexcept {

    auto const heights   = std::move(map.heights);
    auto const ageMap    = std::move(map.ageMap);
    auto const platesMap = std::move(map.platesMap);

    auto const width  = map.width;
    auto const height = map.height;

    auto newHeights   = std::make_unique<f32[]>(width * height);
    auto newAgeMap    = std::make_unique<u32[]>(width * height);
    auto newPlatesMap = std::make_unique<u32[]>(width * height);

    std::vector rowSums(height, 0.0f);
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            rowSums[y] += heights[y * width + x];
        }
    }
    const u32 yWithMinSum = std::distance(rowSums.begin(), std::min_element(rowSums.begin(), rowSums.end()));

    std::vector colSums(width, 0.0f);
    for (u32 x = 0; x < width; x++) {
        for (u32 y = 0; y < height; y++) {
            colSums[x] += heights[y * width + x];
        }
    }
    const u32 xWithMinSum = std::distance(colSums.begin(), std::min_element(colSums.begin(), colSums.end()));

    auto const xOffset = xWithMinSum;
    auto const yOffset = yWithMinSum;

    auto newX = 0;
    auto newY = 0;

    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            newX = (x + xOffset) % width;
            newY = (y + yOffset) % height;

            newHeights[y * width + x]   = heights[newY * width + newX];
            newAgeMap[y * width + x]    = ageMap[newY * width + newX];
            newPlatesMap[y * width + x] = platesMap[newY * width + newX];
        }
    }

    return MapResult(std::move(newHeights), std::move(newAgeMap), std::move(newPlatesMap), width, height);
}

MapResult Aligner::applyBorders(MapResult&& map) noexcept {
    auto heights = std::move(map.heights);
    const uint32_t oceanBorder = std::min(30u, std::max(map.width / 5, map.height / 5));

    for (uint32_t x = 0; x < map.width; ++x) {
        for (uint32_t i = 0; i < oceanBorder; ++i) {

            const uint32_t topIndex = i * map.width + x;
            heights[topIndex] *= static_cast<float>(i) / oceanBorder;

            const uint32_t bottomIndex = (map.height - i - 1) * map.width + x;
            heights[bottomIndex] *= static_cast<float>(i) / oceanBorder;
        }
    }

    return MapResult(std::move(heights), std::move(map.ageMap), std::move(map.platesMap), map.width, map.height);
}
