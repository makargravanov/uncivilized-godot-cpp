//
// Created by Alex on 27.06.2025.
//

#include "PlayScene.h"
void PlayScene::_bind_methods() {
//    godot::ClassDB::bind_method(godot::D_METHOD("set_player"), &PlayScene::set_player);
}
void PlayScene::_ready() {
    Node::_ready();
    Node* camera_node = get_node<Node>("/root/Play/Camera3D");
    if (camera_node && camera_node->has_signal("player_position_updated")) {
        godot::Callable callback = callable_mp(this, &PlayScene::position_updated);
        camera_node->connect("player_position_updated", callback);
    }
}
void PlayScene::_process(double delta) {
    Node::_process(delta);
}
void PlayScene::position_updated(const godot::Variant& position) {
    godot::Vector3 new_position = position;
    godot::UtilityFunctions::print("Camera position updated: ", new_position);
}


