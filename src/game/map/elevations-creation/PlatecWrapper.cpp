//
// Created by Alex on 11.06.2025.
//

#include "PlatecWrapper.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>

#include "../outer-libs/mindwerks-plate-tectonics/platecapi.hpp"
#include "godot_cpp/core/print_string.hpp"

MapResult PlatecWrapper::createHeights(const MapArgs& args, const ProgressCallback& progressCallback = nullptr) noexcept {

    void* api = platec_api_create(args.seed, args.width, args.height, args.seaLevel, args.erosionPeriod, args.foldingRatio,
                args.aggrOverlapAbs, args.aggrOverlapRel, args.cycleCount, args.numPlates);

    if (!api) {
        return {};
    }

    u8 counter = 0;
    f32 currentProgress = 0;
    u8 cycleNumber = 0;

    while (platec_api_is_finished(api) != 1) {
        if (counter==60) {
            counter = 0;
            currentProgress = static_cast<f32>(platec_api_get_iter_count(api))/600;
            cycleNumber = platec_api_get_cycle_count(api);
            progressCallback(currentProgress, cycleNumber);
        }
        counter++;
        platec_api_step(api);
    }

    if (progressCallback) {
        progressCallback(1.0f, platec_api_get_cycle_count(api));
    }

    auto const numElements = args.width * args.height;
    auto const heights        = platec_api_get_heightmap(api);
    auto const ageMap  = platec_api_get_agemap(api);
    auto const platesMap   = platec_api_get_platesmap(api);

    auto heightsCopy   = std::make_unique<f32[]>(numElements);
    auto ageMapCopy    = std::make_unique<u32[]>(numElements);
    auto platesMapCopy = std::make_unique<u16[]>(numElements);

    std::memcpy(heightsCopy.get(), heights, numElements * sizeof(f32));
    std::memcpy(ageMapCopy.get(), ageMap, numElements * sizeof(u32));
    std::memcpy(platesMapCopy.get(), platesMap, numElements * sizeof(u16));

    platec_api_destroy(api);
    return {
        args.seed,
        std::move(heightsCopy),
        std::move(ageMapCopy),
        std::move(platesMapCopy),
        args.width,
        args.height,
        args.seaLevel
    };
}
