//
// Created by Alex on 11.06.2025.
//

#include "MapCreator.h"

#include <cstring>

#include "mindwerks-plate-tectonics/platecapi.hpp"

fun MapCreator::createAsync(const MapArgs& args) noexcept {

    return std::async(std::launch::async, [=]{
        void* api = platec_api_create(args.seed, args.width, args.height, args.seaLevel, args.erosionPeriod, args.foldingRatio,
                    args.aggrOverlapAbs, args.aggrOverlapRel, args.cycleCount, args.numPlates);

        if (!api) {
            return MapResult{};
        }

        while (platec_api_is_finished(api) != 1) {
            platec_api_step(api);
        }

        val numElements = args.width * args.height;
        val heights        = platec_api_get_heightmap(api);
        val ageMap  = platec_api_get_agemap(api);

        var heightsCopy = std::make_unique<f32[]>(numElements);
        var ageMapCopy  = std::make_unique<u32[]>(numElements);

        std::memcpy(heightsCopy.get(), heights, numElements * sizeof(f32));
        std::memcpy(ageMapCopy.get(), ageMap, numElements * sizeof(u32));

        platec_api_destroy(api);

        return MapResult{
            std::move(heightsCopy),
            std::move(ageMapCopy),
            args.width,
            args.height
        };

    });
}