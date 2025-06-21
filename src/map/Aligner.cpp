//
// Created by Alex on 11.06.2025.
//

#include "Aligner.h"

#include "PlatecWrapper.h"
#include <algorithm>
#include <vector>

MapResult Aligner::applyAlign(MapResult&& m) {

        auto const heights       = std::move(m.heights);
        auto const ageMap    = std::move(m.ageMap);
        auto const platesMap = std::move(m.platesMap);

        auto const width  = m.width;
        auto const height = m.height;

        auto newHeights   = std::make_unique<f32[]>(width * height);
        auto newAgeMap    = std::make_unique<u32[]>(width * height);
        auto newPlatesMap = std::make_unique<u32[]>(width * height);

        std::vector rowSums(height, 0.0f);
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                rowSums[y] += heights[y * width + x];
            }
        }
        const u32 yWithMinSum = std::distance(rowSums.begin(),
                                        std::min_element(rowSums.begin(), rowSums.end()));

        std::vector colSums(width, 0.0f);
        for (u32 x = 0; x < width; x++) {
            for (u32 y = 0; y < height; y++) {
                colSums[x] += heights[y * width + x];
            }
        }
        const u32 xWithMinSum = std::distance(colSums.begin(),
                                        std::min_element(colSums.begin(), colSums.end()));

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

        return MapResult(
            std::move(newHeights),
            std::move(newAgeMap),
            std::move(newPlatesMap),
            width,
            height
        );
}
