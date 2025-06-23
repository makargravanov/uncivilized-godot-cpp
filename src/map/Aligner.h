//
// Created by Alex on 11.06.2025.
//

#ifndef GODOT_CPP_TEMPLATE_ALIGNER_H
#define GODOT_CPP_TEMPLATE_ALIGNER_H


struct MapResult;
class Aligner {
public:
    static MapResult applyAlign(MapResult&& map) noexcept;
    static MapResult applyBorders(MapResult&& map) noexcept;
};


#endif //GODOT_CPP_TEMPLATE_ALIGNER_H
