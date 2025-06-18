//
// Created by Alex on 11.06.2025.
//

#ifndef GODOT_CPP_TEMPLATE_ALIGNER_H
#define GODOT_CPP_TEMPLATE_ALIGNER_H
#include "../declarations.h"
#include <future>


struct MapResult;
class Aligner {

    static fun applyAlign(MapResult&& map) noexcept -> std::future<MapResult>;
};


#endif //GODOT_CPP_TEMPLATE_ALIGNER_H
