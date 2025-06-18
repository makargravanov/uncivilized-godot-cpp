//
// Created by Alex on 11.06.2025.
//

#include "MapCreator.h"

#include <cstring>

#include "mindwerks-plate-tectonics/platecapi.hpp"

fun MapCreator::createAsync(const MapArgs& args, ProgressCallback progressCallback = nullptr) noexcept {

    return std::async(std::launch::async, [args, progressCallback]{
        void* api = platec_api_create(args.seed, args.width, args.height, args.seaLevel, args.erosionPeriod, args.foldingRatio,
                    args.aggrOverlapAbs, args.aggrOverlapRel, args.cycleCount, args.numPlates);

        if (!api) {
            return MapResult{};
        }

        u8 counter = 0;
        f32 currentProgress = 0;
        u8 cycleNumber = 0;

        while (platec_api_is_finished(api) != 1) {
            if (counter==60) {
                counter = 0;
                currentProgress = static_cast<f32>(platec_api_get_iter_count(api))/(600*args.cycleCount);
                cycleNumber = platec_api_get_cycle_count(api);
                progressCallback(currentProgress, cycleNumber);
            }
            counter++;
            platec_api_step(api);
        }

        if (progressCallback) {
            progressCallback(1.0f, platec_api_get_cycle_count(api));
        }

        val numElements = args.width * args.height;
        val heights        = platec_api_get_heightmap(api);
        val ageMap  = platec_api_get_agemap(api);
        val platesMap   = platec_api_get_platesmap(api);

        var heightsCopy   = std::make_unique<f32[]>(numElements);
        var ageMapCopy    = std::make_unique<u32[]>(numElements);
        var platesMapCopy = std::make_unique<u32[]>(numElements);

        std::memcpy(heightsCopy.get(), heights, numElements * sizeof(f32));
        std::memcpy(ageMapCopy.get(), ageMap, numElements * sizeof(u32));
        std::memcpy(platesMapCopy.get(), platesMap, numElements * sizeof(u32));

        platec_api_destroy(api);

        return MapResult {
            std::move(heightsCopy),
            std::move(ageMapCopy),
            std::move(platesMapCopy),
            args.width,
            args.height
        };

    });
}