//
// Created by Alex on 27.06.2025.
//


#ifndef MAPMANAGER_H
#define MAPMANAGER_H

#include "util/declarations.h"
#include "util/GodotPtr.h"
#include "TileData.h"
#include "ViewMode.h"
#include "game/map/LandTypeConfig.h"

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <unordered_map>

#include "godot_cpp/variant/variant.hpp"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

constexpr u8 CHUNK_SIZE = 64;

template <>
struct std::hash<godot::Vector2i> {
    size_t operator()(const godot::Vector2i& point) const noexcept {
        return static_cast<size_t>(point.x) ^ (static_cast<size_t>(point.y) << 16);
    }
};

struct LandTypeMeshData {
    GodotPtr<godot::MultiMeshInstance3D> instance;
    godot::Ref<godot::MultiMesh> multiMesh;
    godot::Ref<godot::ShaderMaterial> material;

    // Per-instance tile index for overlay updates.
    std::vector<i32> tileIndices;

    LandTypeMeshData() = default;

    LandTypeMeshData(LandTypeMeshData&&) = default;
    LandTypeMeshData& operator=(LandTypeMeshData&&) = default;

    LandTypeMeshData(const LandTypeMeshData&) = delete;
    LandTypeMeshData& operator=(const LandTypeMeshData&) = delete;

    void create();
    bool initialize(i32 instanceCount, const char* meshPath, const char* materialPath);
    void addToScene() const;
    void removeFromScene() const;

    void setInstanceTransform(i32 index, const godot::Transform3D& transform) const {
        if (multiMesh.is_valid()) {
            multiMesh->set_instance_transform(index, transform);
        }
    }

    void setInstanceCustomData(i32 index, const godot::Color& data) const {
        if (multiMesh.is_valid()) {
            multiMesh->set_instance_custom_data(index, data);
        }
    }
};

struct Chunk {
    godot::Vector3 chunkPos;
    std::map<ReliefType, LandTypeMeshData> meshes;

    Chunk() = default;

    Chunk(Chunk&&) = default;
    Chunk& operator=(Chunk&&) = default;

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    ~Chunk();

    void initialize(
        const godot::Vector3& position,
        const std::map<ReliefType, i32>& typeCounts
    );
};

// Callback: given a tile index, return the overlay value [0,1] for current ViewMode.
using OverlayFunc = std::function<f32(i32 tileIndex)>;

class MapManager {
public:
    explicit MapManager(std::unique_ptr<TileData[]> tileData, u16 width, u16 height)
        : tiles(std::move(tileData)),
          gridWidth(width),
          gridHeight(height) {}

    MapManager(const MapManager&) = delete;
    MapManager& operator=(const MapManager&) = delete;
    MapManager(MapManager&&) noexcept = delete;
    MapManager& operator=(MapManager&&) noexcept = delete;

    void unloadChunk(godot::Vector2i vec);
    void loadChunk(godot::Vector2i vec);
    void updateVisibleChunks(const godot::Vector3& playerPosition);
    static bool isChunkInRenderDistance(const godot::Vector2i& chunkPos, const godot::Vector2i& playerChunkPos);
    void positionUpdated(const godot::Variant& position);
    static godot::Vector3 getChunkPos(const godot::Vector3& worldPos);

    // Overlay system: switch view mode and rewrite .w on all loaded chunks.
    void setViewMode(ViewMode mode, const OverlayFunc& overlayFunc);

    // Re-read tile data and update INSTANCE_CUSTOM on all loaded chunks.
    void refreshAllInstances();

    const TileData& getTile(i32 index) const { return tiles[index]; }
    TileData* getTilesPtr() { return tiles.get(); }

private:
    std::unordered_map<godot::Vector2i, Chunk> loadedChunks;
    std::future<void> futureResult;
    std::unique_ptr<TileData[]> tiles;
    ViewMode currentViewMode = VIEW_NORMAL;
    OverlayFunc currentOverlayFunc;

public:
    static f32 tileHorizontalOffset;
    static f32 oddRowHorizontalOffset;
    static f32 tileVerticalOffset;
    u16 gridWidth = 0;
    u16 gridHeight = 0;
    static u8 renderDistance;

    static godot::Transform3D calculateTileTransform(i32 x, i32 y) {
        const bool oddRow = std::abs(y % 2) == 1;
        const f32 xPos = oddRow
            ? x * tileHorizontalOffset + oddRowHorizontalOffset
            : x * tileHorizontalOffset;
        constexpr f32 yPos = 0.0f;
        const f32 zPos = y * tileVerticalOffset;

        return godot::Transform3D(
            godot::Basis(),
            godot::Vector3(xPos, yPos, zPos)
        );
    }
};

#endif //MAPMANAGER_H
