//
// Created by Alex on 27.06.2025.
//

#ifndef SYSTEMNEXUS_H
#define SYSTEMNEXUS_H
#include "map/MapManager.h"
#include "map/elevations-creation/LayerSeparator.h"

class SystemNexus {
public:
    static void configureMap(SeparatedMapResult mapResult) {
        mapManager = new MapManager(std::move(mapResult));
    }
    static MapManager* getMapManager() {
        return mapManager;
    }

    static void finalize() {
        delete mapManager;
    }

private:
    static MapManager* mapManager;
};

#endif //SYSTEMNEXUS_H
