//
// Created by Alex on 27.06.2025.
//

#ifndef SYSTEMNEXUS_H
#define SYSTEMNEXUS_H
#include "api-classes/PlayScene.h"
#include "map/MapManager.h"
#include "map/elevations-creation/LayerSeparator.h"

//FIXME: возможно, стоит улучшить, сделать нормальную регистрацию и авто-отписку через RAII-объект, но пока норм, хоть и костыльно
class SystemNexus {
public:
    static void configureMap(SeparatedMapResult mapResult) {
        mapManager = new MapManager(std::move(mapResult));
    }
    static MapManager* getMapManager() {
        return mapManager;
    }

    static void regPlayScene(PlayScene* p) {
        play = p;
    }
    static PlayScene* playScene() {
        return play;
    }

    static void finalize() {
        delete mapManager;
    }

private:
    static MapManager* mapManager;
    static PlayScene* play;
};

#endif //SYSTEMNEXUS_H
