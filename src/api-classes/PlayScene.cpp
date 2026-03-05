//
// Created by Alex on 27.06.2025.
//

#include "PlayScene.h"
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>

#include "game/SystemNexus.h"
#include "game/map/ViewMode.h"
void PlayScene::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_view_mode", "mode"), &PlayScene::set_view_mode);
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

void PlayScene::set_view_mode(int mode) {
    if (!mapManager) return;

    auto viewMode = static_cast<ViewMode>(
        mode < 0 ? 0 : (mode >= VIEW_MODE_COUNT ? 0 : mode));

    OverlayFunc func;
    if (viewMode == VIEW_ELEVATION) {
        // Demo overlay: use raw biome_id normalized to [0,1] as placeholder.
        // Will be replaced with real data (temperature, moisture) when ClimateModel exists.
        func = [this](i32 tileIndex) -> f32 {
            const auto& tile = mapManager->getTile(tileIndex);
            return static_cast<f32>(tile.biome) / 6.0f;
        };
    } else if (viewMode != VIEW_NORMAL) {
        // Placeholder for future modes — return 0.
        func = [](i32) -> f32 { return 0.0f; };
    }

    mapManager->setViewMode(viewMode, func);
}




