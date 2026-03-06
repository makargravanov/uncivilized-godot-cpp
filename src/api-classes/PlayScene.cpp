//
// Created by Alex on 27.06.2025.
//

#include "PlayScene.h"
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>

#include "game/SystemNexus.h"
#include "game/map/ViewMode.h"
#include "game/map/BiomeClassifier.h"
#include "game/map/climate/ClimateModel.h"

void PlayScene::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_view_mode", "mode"), &PlayScene::set_view_mode);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tile_info", "world_x", "world_z"), &PlayScene::get_tile_info);
    godot::ClassDB::bind_method(godot::D_METHOD("advance_turn"), &PlayScene::advance_turn);
    godot::ClassDB::bind_method(godot::D_METHOD("get_climate_metrics"), &PlayScene::get_climate_metrics);
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

    auto* climate = SystemNexus::getClimateModel();

    OverlayFunc func;
    if (viewMode == VIEW_TEMPERATURE && climate) {
        // Temperature overlay: annual mean, map −40..+40 °C → 0..1 (blue→red).
        func = [climate](i32 tileIndex) -> f32 {
            const f32 t = climate->getAnnualTemperatureCelsius(tileIndex);
            return std::clamp((t + 40.0f) / 80.0f, 0.0f, 1.0f);
        };
    } else if (viewMode == VIEW_MOISTURE && climate) {
        // Precipitation overlay: annual total, 0..2000 mm/year → 0..1.
        func = [climate](i32 tileIndex) -> f32 {
            const f32 p = climate->getAnnualPrecipitation(tileIndex);
            return std::clamp(p / 2000.0f, 0.0f, 1.0f);
        };
    } else if (viewMode == VIEW_ELEVATION && climate) {
        // Elevation overlay: 0..6000m → 0..1.
        func = [climate](i32 tileIndex) -> f32 {
            const f32 e = climate->getElevation(tileIndex);
            return std::clamp(e / 6000.0f, 0.0f, 1.0f);
        };
    } else if (viewMode == VIEW_BIOME) {
        // Biome ID normalized to [0,1].
        func = [this](i32 tileIndex) -> f32 {
            const auto& tile = mapManager->getTile(tileIndex);
            return static_cast<f32>(tile.biome) / static_cast<f32>(BIOME_TYPE_COUNT - 1);
        };
    } else if (viewMode != VIEW_NORMAL) {
        func = [](i32) -> f32 { return 0.0f; };
    }

    mapManager->setViewMode(viewMode, func);
}

godot::Dictionary PlayScene::get_tile_info(float world_x, float world_z) {
    godot::Dictionary info;
    if (!mapManager) return info;

    // Convert world position → hex offset coordinates.
    const i32 y = static_cast<i32>(std::round(world_z / MapManager::tileVerticalOffset));
    const bool oddRow = std::abs(y % 2) == 1;
    const f32 x_offset = oddRow ? MapManager::oddRowHorizontalOffset : 0.0f;
    const i32 x = static_cast<i32>(std::round((world_x - x_offset) / MapManager::tileHorizontalOffset));

    if (x < 0 || x >= mapManager->gridWidth || y < 0 || y >= mapManager->gridHeight)
        return info;

    const i32 index = y * mapManager->gridWidth + x;
    const auto& tile = mapManager->getTile(index);
    auto* climate = SystemNexus::getClimateModel();

    info["biome"] = static_cast<int>(tile.biome);
    info["relief"] = static_cast<int>(tile.relief);
    info["temperature"] = climate ? climate->getTemperatureCelsius(index) : static_cast<f32>(tile.temperature);
    info["temperature_annual"] = climate ? climate->getAnnualTemperatureCelsius(index) : static_cast<f32>(tile.temperature);
    info["precipitation"] = climate ? climate->getPrecipitation(index) : static_cast<f32>(tile.precipitation);
    info["precipitation_annual"] = climate ? climate->getAnnualPrecipitation(index) : static_cast<f32>(tile.precipitation);
    info["elevation"] = climate ? climate->getElevation(index) : 0.0f;
    info["x"] = x;
    info["y"] = y;

    return info;
}

void PlayScene::advance_turn() {
    auto* climate = SystemNexus::getClimateModel();
    if (!climate || !mapManager) return;

    constexpr f32 dt = 1.0f / 48.0f;
    climate->tick(dt);

    auto* tiles = mapManager->getTilesPtr();
    const u32 total = static_cast<u32>(mapManager->gridWidth) * mapManager->gridHeight;
    BiomeClassifier::reclassifyFromClimate(tiles, *climate, total);
    climate->writeToTiles(tiles, total);

    // Re-apply current overlay on all loaded chunks so visuals update.
    mapManager->refreshAllInstances();
}

godot::Dictionary PlayScene::get_climate_metrics() {
    godot::Dictionary d;
    auto* climate = SystemNexus::getClimateModel();
    if (!climate) return d;

    const auto& m = climate->getLastMetrics();

    d["absorbed_solar"]   = m.absorbed_solar;
    d["emitted_ir"]       = m.emitted_ir;
    d["toa_imbalance"]    = m.toa_imbalance;
    d["T_mean_global"]    = m.T_mean_global;
    d["T_mean_land"]      = m.T_mean_land;
    d["T_mean_ocean"]     = m.T_mean_ocean;
    d["T_min"]            = m.T_min;
    d["T_max"]            = m.T_max;
    d["dT_mean"]          = m.dT_mean;
    d["ice_fraction"]     = m.ice_fraction;
    d["snow_fraction"]    = m.snow_fraction;
    d["moisture_mean"]    = m.moisture_mean;
    d["precip_mean_land"] = m.precip_mean_land;
    d["albedo_mean"]      = m.albedo_mean;
    d["land_cells"]       = static_cast<int>(m.land_cells);
    d["ocean_cells"]      = static_cast<int>(m.ocean_cells);
    d["year"]             = climate->getYearFraction();

    return d;
}
