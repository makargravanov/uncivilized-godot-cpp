//
// Created by Alex on 11.06.2025.
//

#include "Aligner.h"

#include "MapCreator.h"
#include <algorithm>
#include <vector>

fun Aligner::applyAlign(MapResult&& map) {

    return std::async(std::launch::async, [m = std::move(map)] mutable {
        val heights       = std::move(m.heights);
        val ageMap    = std::move(m.ageMap);
        val platesMap = std::move(m.platesMap);

        val width  = m.width;
        val height = m.height;


        var newHeights   = std::make_unique<f32[]>(width * height);
        var newAgeMap    = std::make_unique<u32[]>(width * height);
        var newPlatesMap = std::make_unique<u32[]>(width * height);

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

        val xOffset = xWithMinSum;
        val yOffset = yWithMinSum;

        var newX = 0;
        var newY = 0;

        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                newX = (x + xOffset) % width;
                newY = (y + yOffset) % height;

                newHeights[y * width + x]   = heights[newY * width + newX];
                newAgeMap[y * width + x]    = ageMap[newY * width + newX];
                newPlatesMap[y * width + x] = platesMap[newY * width + newX];
            }
        }

        return MapResult{
            std::move(newHeights),
            std::move(newAgeMap),
            std::move(newPlatesMap),
            width,
            height
        };
    });
}
