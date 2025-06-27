//
// Created by Alex on 27.06.2025.
//

#ifndef PLAYSCENE_H
#define PLAYSCENE_H

#include <future>

#include <godot_cpp/classes/node.hpp>

class PlayScene : public godot::Node{
    GDCLASS(PlayScene, Node)
private:
    std::future<void> futureResult;


protected:
    static void _bind_methods();

public:
    void _ready() override;
    void _process(double delta) override;

    void position_updated(const godot::Variant& position);
};



#endif //PLAYSCENE_H
