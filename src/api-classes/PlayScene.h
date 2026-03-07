//
// Created by Alex on 27.06.2025.
//

#ifndef PLAYSCENE_H
#define PLAYSCENE_H

#include "util/declarations.h"
#include "game/map/ViewMode.h"
#include "util/GodotPtr.h"
#include <future>

#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

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

    // Overlay system — call from GDScript: play_scene.set_view_mode(...)
    void set_view_mode(int mode);

    // Returns tile info at world (x, z) or empty dict if out of bounds.
    godot::Dictionary get_tile_info_at(float worldX, float worldZ);

    // Start computing the next climate turn in the background if no turn is pending.
    void advance_climate_turn();
    bool is_climate_turn_in_progress() const;
    int get_current_turn() const;

private:
    void ensureWindDebugNode();
    void refreshWindDebugView();
    void setWindDebugVisible(bool visible);

    MapManager* mapManager = nullptr;
    ViewMode currentViewMode = VIEW_NORMAL;
    GodotPtr<godot::MeshInstance3D> windDebugInstance;
    godot::Ref<godot::ImmediateMesh> windDebugMesh;
    godot::Ref<godot::StandardMaterial3D> windDebugMaterial;
};



#endif //PLAYSCENE_H
