//
// Created by Alex on 27.06.2025.
//

#ifndef PLAYSCENE_H
#define PLAYSCENE_H

#include "../util/declarations.h"
#include <future>

#include <godot_cpp/classes/node.hpp>

#define CHUNK_SIZE 64

class PlayScene : public godot::Node{
    GDCLASS(PlayScene, Node)

protected:
    static void _bind_methods();

public:
    void _ready() override;
    void _process(double delta) override;

    void position_updated(const godot::Variant& position);

private:
    std::future<void> futureResult;
    f32 tileHorizontalOffset   = 173.205078;
    f32 oddRowHorizontalOffset = 86.602539;
    f32 tileVerticalOffset     = 150.0;
};



#endif //PLAYSCENE_H
