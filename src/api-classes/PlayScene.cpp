//
// Created by Alex on 27.06.2025.
//

#include "PlayScene.h"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "game/climate/TemperaturePass.h"
#include "game/climate/MoisturePass.h"
#include "game/climate/ClimateState.h"
#include "game/climate/ClimateConfig.h"
#include "game/SystemNexus.h"
#include "game/map/BiomeType.h"
#include "game/map/ViewMode.h"

namespace {

constexpr f32 KELVIN_OFFSET = ClimateSettings::DEFAULT_CLIMATE_CONFIG.shared.kelvinOffset;

constexpr i32 WIND_DEBUG_STRIDE = 2;
constexpr f32 WIND_DEBUG_BASE_HEIGHT = 0.3f;
constexpr f32 WIND_DEBUG_HILL_HEIGHT = 0.54f;
constexpr f32 WIND_DEBUG_MOUNTAIN_HEIGHT = 0.92f;
constexpr f32 WIND_DEBUG_SCALE = 0.12f;
constexpr f32 WIND_DEBUG_MIN_LENGTH = 0.12f;
constexpr f32 WIND_DEBUG_MAX_LENGTH = 0.95f;
constexpr f32 WIND_DEBUG_HEAD_LENGTH = 0.22f;
constexpr f32 WIND_DEBUG_HEAD_WIDTH = 0.15f;
constexpr f32 WIND_DEBUG_COLOR_COLD = 0.35f;
constexpr f32 WIND_DEBUG_COLOR_HOT = 0.95f;
constexpr f32 WIND_DEBUG_COLOR_ALPHA = 0.95f;

f32 getWindDebugHeight(const ReliefType relief) {
    switch (relief) {
        case RELIEF_HILL:
            return WIND_DEBUG_HILL_HEIGHT;
        case RELIEF_MOUNTAIN:
        case RELIEF_HIGH_MOUNTAIN:
            return WIND_DEBUG_MOUNTAIN_HEIGHT;
        case RELIEF_OCEAN:
        case RELIEF_LAND:
        default:
            return WIND_DEBUG_BASE_HEIGHT;
    }
}

godot::Color getWindDebugColor(const f32 magnitude) {
    const f32 normalized = std::clamp(magnitude / 9.0f, 0.0f, 1.0f);
    return godot::Color(
        0.15f + normalized * 0.75f,
        0.35f + normalized * 0.4f,
        WIND_DEBUG_COLOR_COLD + (1.0f - normalized) * (WIND_DEBUG_COLOR_HOT - WIND_DEBUG_COLOR_COLD),
        WIND_DEBUG_COLOR_ALPHA);
}

} // namespace

void PlayScene::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_view_mode", "mode"), &PlayScene::set_view_mode);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tile_info_at", "worldX", "worldZ"), &PlayScene::get_tile_info_at);
    godot::ClassDB::bind_method(godot::D_METHOD("advance_climate_turn"), &PlayScene::advance_climate_turn);
    godot::ClassDB::bind_method(godot::D_METHOD("is_climate_turn_in_progress"), &PlayScene::is_climate_turn_in_progress);
    godot::ClassDB::bind_method(godot::D_METHOD("get_current_turn"), &PlayScene::get_current_turn);
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

    ensureWindDebugNode();
    setWindDebugVisible(false);
}
void PlayScene::_process(double delta) {
    Node::_process(delta);

    if (SystemNexus::applyCompletedClimateTurn()) {
        set_view_mode(static_cast<int>(currentViewMode));
    }
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

    currentViewMode = static_cast<ViewMode>(
        mode < 0 ? 0 : (mode >= VIEW_MODE_COUNT ? 0 : mode));

    OverlayFunc func;
    if (currentViewMode == VIEW_TEMPERATURE) {
        func = [this](i32 tileIndex) -> f32 {
            if (const ClimateState* climateState = SystemNexus::getClimateState();
                climateState && climateState->temperatureKelvin) {
                return TemperaturePass::normalizeKelvinForOverlay(climateState->temperatureKelvin[tileIndex]);
            }

            const auto& tile = mapManager->getTile(tileIndex);
            return TemperaturePass::normalizeForOverlay(tile.temperature);
        };
    } else if (currentViewMode == VIEW_MOISTURE) {
        func = [](i32 tileIndex) -> f32 {
            if (const ClimateState* climateState = SystemNexus::getClimateState();
                climateState && climateState->humidityKgPerKg) {
                return MoisturePass::normalizeForOverlay(climateState->humidityKgPerKg[tileIndex]);
            }
            return 0.0f;
        };
    } else if (currentViewMode == VIEW_PRECIPITATION) {
        func = [](i32 tileIndex) -> f32 {
            if (const ClimateState* climateState = SystemNexus::getClimateState();
                climateState && climateState->turnPrecipitation) {
                return MoisturePass::normalizePrecipitationForOverlay(
                    climateState->turnPrecipitation[tileIndex]);
            }
            return 0.0f;
        };
    } else if (currentViewMode == VIEW_BIOME) {
        func = [this](i32 tileIndex) -> f32 {
            const auto& tile = mapManager->getTile(tileIndex);
            return static_cast<f32>(tile.biome) / static_cast<f32>(BIOME_TYPE_COUNT - 1);
        };
    } else if (currentViewMode == VIEW_ELEVATION) {
        // Demo overlay: use raw biome_id normalized to [0,1] as placeholder.
        func = [this](i32 tileIndex) -> f32 {
            const auto& tile = mapManager->getTile(tileIndex);
            return static_cast<f32>(tile.biome) / static_cast<f32>(BIOME_TYPE_COUNT - 1);
        };
    } else if (currentViewMode != VIEW_NORMAL && currentViewMode != VIEW_WIND
               && currentViewMode != VIEW_PRECIPITATION) {
        // Placeholder for future modes — return 0.
        func = [](i32) -> f32 { return 0.0f; };
    }

    const ViewMode tileViewMode = (currentViewMode == VIEW_WIND) ? VIEW_NORMAL : currentViewMode;
    mapManager->setViewMode(tileViewMode, func);
    setWindDebugVisible(currentViewMode == VIEW_WIND);
    if (currentViewMode == VIEW_WIND) {
        refreshWindDebugView();
    }
}

void PlayScene::advance_climate_turn() {
    SystemNexus::requestClimateTurnAsync();
}

bool PlayScene::is_climate_turn_in_progress() const {
    return SystemNexus::isClimateTurnInProgress();
}

int PlayScene::get_current_turn() const {
    return static_cast<int>(SystemNexus::currentClimateTurn());
}

godot::Dictionary PlayScene::get_tile_info_at(const float worldX, const float worldZ) {
    godot::Dictionary result;
    if (!mapManager) return result;

    const i32 row = static_cast<i32>(std::round(worldZ / MapManager::tileVerticalOffset));
    const bool oddRow = std::abs(row % 2) == 1;
    const f32 adjustedX = oddRow ? worldX - MapManager::oddRowHorizontalOffset : worldX;
    const i32 col = static_cast<i32>(std::round(adjustedX / MapManager::tileHorizontalOffset));

    if (row < 0 || row >= mapManager->gridHeight || col < 0 || col >= mapManager->gridWidth) {
        return result;
    }

    const i32 tileIndex = row * mapManager->gridWidth + col;
    const TileData& tile = mapManager->getTile(tileIndex);

    result["col"] = col;
    result["row"] = row;
    result["relief"] = static_cast<int>(tile.relief);
    result["biome"] = static_cast<int>(tile.biome);
    result["temperature_c"] = static_cast<int>(tile.temperature);

    if (const ClimateState* cs = SystemNexus::getClimateState(); cs) {
        result["turn"] = static_cast<int>(cs->absoluteTurnIndex);
        if (cs->temperatureKelvin)
            result["temperature_k"] = cs->temperatureKelvin[tileIndex];
        if (cs->windEastMps && cs->windNorthMps) {
            const f32 we = cs->windEastMps[tileIndex];
            const f32 wn = cs->windNorthMps[tileIndex];
            result["wind_east"] = we;
            result["wind_north"] = wn;
            result["wind_speed"] = std::sqrt(we * we + wn * wn);
        }
        if (cs->latitudeRadians)
            result["latitude_deg"] = cs->latitudeRadians[tileIndex] * (180.0f / 3.14159265f);
        if (cs->relativeAltitude)
            result["altitude"] = cs->relativeAltitude[tileIndex];
        if (cs->humidityKgPerKg)
            result["humidity"] = cs->humidityKgPerKg[tileIndex];
        if (cs->turnPrecipitation)
            result["precipitation_turn"] = cs->turnPrecipitation[tileIndex];
        if (cs->annualPrecipitationAccumulator)
            result["precipitation_annual"] = cs->annualPrecipitationAccumulator[tileIndex];
        if (cs->forestCoverFraction)
            result["forest_cover_fraction"] = cs->forestCoverFraction[tileIndex];
        if (cs->surfaceAlbedo)
            result["surface_albedo"] = cs->surfaceAlbedo[tileIndex];
        if (cs->effectiveHeatCapacity)
            result["effective_heat_capacity"] = cs->effectiveHeatCapacity[tileIndex];
        if (cs->snowWaterEquivalent)
            result["snow_water_equivalent"] = cs->snowWaterEquivalent[tileIndex];
        if (cs->snowCoverFraction)
            result["snow_cover_fraction"] = cs->snowCoverFraction[tileIndex];
        if (cs->seaIceFraction)
            result["sea_ice_fraction"] = cs->seaIceFraction[tileIndex];
        result["climate_years_completed"] = static_cast<int>(cs->completedClimateYears);
        if (cs->completedClimateYears > 0) {
            if (cs->completedAnnualPrecipitation)
                result["precipitation_annual_completed"] = cs->completedAnnualPrecipitation[tileIndex];
            if (cs->completedAnnualMeanTemperatureKelvin)
                result["temperature_annual_mean_c"] =
                    cs->completedAnnualMeanTemperatureKelvin[tileIndex] - KELVIN_OFFSET;
            if (cs->completedAnnualTemperatureMinKelvin)
                result["temperature_annual_min_c"] =
                    cs->completedAnnualTemperatureMinKelvin[tileIndex] - KELVIN_OFFSET;
            if (cs->completedAnnualTemperatureMaxKelvin)
                result["temperature_annual_max_c"] =
                    cs->completedAnnualTemperatureMaxKelvin[tileIndex] - KELVIN_OFFSET;
            if (cs->completedAnnualTemperatureMinKelvin && cs->completedAnnualTemperatureMaxKelvin)
                result["temperature_annual_amplitude_c"] =
                    cs->completedAnnualTemperatureMaxKelvin[tileIndex]
                    - cs->completedAnnualTemperatureMinKelvin[tileIndex];
            if (cs->completedColdestQuarterMeanTemperatureKelvin)
                result["temperature_coldest_quarter_c"] =
                    cs->completedColdestQuarterMeanTemperatureKelvin[tileIndex] - KELVIN_OFFSET;
            if (cs->completedWarmestQuarterMeanTemperatureKelvin)
                result["temperature_warmest_quarter_c"] =
                    cs->completedWarmestQuarterMeanTemperatureKelvin[tileIndex] - KELVIN_OFFSET;
            if (cs->completedDriestQuarterPrecipitation)
                result["precipitation_driest_quarter"] =
                    cs->completedDriestQuarterPrecipitation[tileIndex];
            if (cs->completedWettestQuarterPrecipitation)
                result["precipitation_wettest_quarter"] =
                    cs->completedWettestQuarterPrecipitation[tileIndex];
            if (cs->completedDriestQuarterPrecipitation && cs->completedWettestQuarterPrecipitation)
                result["precipitation_seasonality_ratio"] =
                    cs->completedWettestQuarterPrecipitation[tileIndex]
                    / std::max(cs->completedDriestQuarterPrecipitation[tileIndex], 1e-4f);
        }
    }

    return result;
}

void PlayScene::ensureWindDebugNode() {
    if (windDebugInstance) {
        return;
    }

    windDebugMesh.instantiate();
    windDebugMaterial.instantiate();

    windDebugMaterial->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
    windDebugMaterial->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    windDebugMaterial->set_flag(godot::BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
    windDebugMaterial->set_cull_mode(godot::BaseMaterial3D::CULL_DISABLED);
    windDebugMaterial->set_transparency(godot::BaseMaterial3D::TRANSPARENCY_ALPHA);

    windDebugInstance = GodotPtr(memnew(godot::MeshInstance3D));
    windDebugInstance->set_name("WindDebugVectors");
    windDebugInstance->set_mesh(windDebugMesh);
    windDebugInstance->set_cast_shadows_setting(godot::GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
    windDebugInstance->set_material_override(windDebugMaterial);
    add_child(windDebugInstance.get());
}

void PlayScene::setWindDebugVisible(const bool visible) {
    if (!windDebugInstance) {
        return;
    }

    windDebugInstance->set_visible(visible);
    if (!visible && windDebugMesh.is_valid()) {
        windDebugMesh->clear_surfaces();
    }
}

void PlayScene::refreshWindDebugView() {
    if (!windDebugMesh.is_valid()) {
        return;
    }

    windDebugMesh->clear_surfaces();

    const ClimateState* climateState = SystemNexus::getClimateState();
    if (!mapManager || !climateState || !climateState->windEastMps || !climateState->windNorthMps) {
        return;
    }

    windDebugMesh->surface_begin(godot::Mesh::PRIMITIVE_LINES, windDebugMaterial);

    for (u32 row = 0; row < mapManager->gridHeight; row += WIND_DEBUG_STRIDE) {
        for (u32 column = 0; column < mapManager->gridWidth; column += WIND_DEBUG_STRIDE) {
            const u32 tileIndex = row * mapManager->gridWidth + column;
            const f32 windEast = climateState->windEastMps[tileIndex];
            const f32 windNorth = climateState->windNorthMps[tileIndex];
            const f32 magnitude = std::sqrt(windEast * windEast + windNorth * windNorth);
            if (magnitude < 1e-3f) {
                continue;
            }

            const godot::Transform3D tileTransform = MapManager::calculateTileTransform(column, row);
            const TileData& tile = mapManager->getTile(static_cast<i32>(tileIndex));
            const godot::Vector3 origin(
                tileTransform.origin.x,
                getWindDebugHeight(tile.relief),
                tileTransform.origin.z);

            const f32 clampedLength = std::clamp(magnitude * WIND_DEBUG_SCALE, WIND_DEBUG_MIN_LENGTH, WIND_DEBUG_MAX_LENGTH);
            // +Z is south in world coords, but windNorth > 0 means northward → negate.
            const godot::Vector3 direction(windEast / magnitude, 0.0f, -windNorth / magnitude);
            const godot::Vector3 tip = origin + direction * clampedLength;
            const godot::Vector3 perpendicular(-direction.z, 0.0f, direction.x);
            const godot::Vector3 headBase = tip - direction * (clampedLength * WIND_DEBUG_HEAD_LENGTH);
            const godot::Vector3 leftWing = headBase + perpendicular * (clampedLength * WIND_DEBUG_HEAD_WIDTH);
            const godot::Vector3 rightWing = headBase - perpendicular * (clampedLength * WIND_DEBUG_HEAD_WIDTH);
            const godot::Color color = getWindDebugColor(magnitude);

            windDebugMesh->surface_set_color(color);
            windDebugMesh->surface_add_vertex(origin);
            windDebugMesh->surface_add_vertex(tip);

            windDebugMesh->surface_set_color(color);
            windDebugMesh->surface_add_vertex(tip);
            windDebugMesh->surface_add_vertex(leftWing);

            windDebugMesh->surface_set_color(color);
            windDebugMesh->surface_add_vertex(tip);
            windDebugMesh->surface_add_vertex(rightWing);
        }
    }

    windDebugMesh->surface_end();
}




