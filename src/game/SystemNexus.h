//
// Created by Alex on 27.06.2025.
//

#ifndef SYSTEMNEXUS_H
#define SYSTEMNEXUS_H
#include "api-classes/PlayScene.h"
#include "map/MapManager.h"
#include "map/BiomeClassifier.h"
#include "map/climate/ClimateModel.h"
#include "map/elevations-creation/LayerSeparator.h"

//FIXME: возможно, стоит улучшить, сделать нормальную регистрацию и авто-отписку через RAII-объект, но пока норм, хоть и костыльно
class SystemNexus {
public:
    static void configureMap(SeparatedMapResult mapResult) {
        const u32 w = mapResult.mapResult.width;
        const u32 h = mapResult.mapResult.height;

        // Phase 1: elevation → relief+biome stub.
        auto tiles = BiomeClassifier::classify(mapResult.discrete, w, h);

        // Phase 2: climate model from raw heightmap.
        climate = new ClimateModel(
            mapResult.mapResult.heights.get(),
            mapResult.mapResult.oceanLevel,
            w, h);

        // Warmup: ~6 full years (288 ticks) to reach near-equilibrium.
        climate->warmup(288);

        // Phase 3: reclassify biomes from climate data.
        BiomeClassifier::reclassifyFromClimate(tiles.get(), *climate, w * h);

        // Phase 4: write T/P into TileData.
        climate->writeToTiles(tiles.get(), w * h);

        mapManager = new MapManager(std::move(tiles), w, h);
    }

    static MapManager* getMapManager() {
        return mapManager;
    }
    static ClimateModel* getClimateModel() {
        return climate;
    }

    static void regPlayScene(PlayScene* p) {
        play = p;
    }
    static PlayScene* playScene() {
        return play;
    }

    static void finalize() {
        delete mapManager;
        delete climate;
    }

private:
    static MapManager* mapManager;
    static ClimateModel* climate;
    static PlayScene* play;
};

#endif //SYSTEMNEXUS_H
