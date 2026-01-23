//
// Created by Alex on 27.06.2025.
//

#ifndef PLAYSCENE_H
#define PLAYSCENE_H

#include "util/declarations.h"
#include <future>

#include <godot_cpp/classes/node.hpp>

namespace godot {
    class MultiMeshInstance3D;
}
class MapManager;

class PlayScene : public godot::Node{
    GDCLASS(PlayScene, Node)

protected:
    static void _bind_methods();

public:
    void _ready() override;
    void _process(double delta) override;

    void position_updated(const godot::Variant& position);
    void addISM(godot::MultiMeshInstance3D* meshInstance);
    void removeISM(godot::MultiMeshInstance3D* meshInstance);

private:
    MapManager* mapManager = nullptr;
};



#endif //PLAYSCENE_H
