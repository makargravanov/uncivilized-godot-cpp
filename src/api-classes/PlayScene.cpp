//
// Created by Alex on 27.06.2025.
//

#include "PlayScene.h"
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>

#include "game/SystemNexus.h"
void PlayScene::_bind_methods() {
//    godot::ClassDB::bind_method(godot::D_METHOD("set_player"), &PlayScene::set_player);
}
void PlayScene::_ready() {
    Node::_ready();
    mapManager = SystemNexus::getMapManager();
    if (mapManager) {
        SystemNexus::regPlayScene(this);
        if (Node* camera_node = get_node<Node>("/root/Play/Camera3D");
            camera_node && camera_node->has_signal("player_position_updated")) {
            const godot::Callable callback = callable_mp(this, &PlayScene::position_updated);
            camera_node->connect("player_position_updated", callback);
        }
    }
}
void PlayScene::_process(double delta) {
    Node::_process(delta);
}
void PlayScene::position_updated(const godot::Variant& position) {
    mapManager->positionUpdated(position);
}
void PlayScene::addISM(godot::MultiMeshInstance3D* meshInstance) {
    add_child(meshInstance);
}
void PlayScene::removeISM(godot::MultiMeshInstance3D* meshInstance) {
    remove_child(meshInstance);
}




