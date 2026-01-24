//
// Created by Alex on 27.06.2025.
//


#ifndef MAPMANAGER_H
#define MAPMANAGER_H

#include "util/declarations.h"
#include "util/GodotPtr.h"
#include "elevations-creation/LayerSeparator.h"
#include "elevations-creation/DiscreteLandTypeByHeight.h"
#include "game/map/LandTypeConfig.h"

#include <future>
#include <map>
#include <memory>
#include <unordered_map>

#include "godot_cpp/variant/variant.hpp"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
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

    LandTypeMeshData() = default;

    LandTypeMeshData(LandTypeMeshData&&) = default;
    LandTypeMeshData& operator=(LandTypeMeshData&&) = default;

    LandTypeMeshData(const LandTypeMeshData&) = delete;
    LandTypeMeshData& operator=(const LandTypeMeshData&) = delete;

    void create();
    bool initialize(i32 instanceCount, const char* meshPath) const;
    void addToScene() const;
    void removeFromScene() const;

    void setInstanceTransform(i32 index, const godot::Transform3D& transform) const {
        if (multiMesh.is_valid()) {
            multiMesh->set_instance_transform(index, transform);
        }
    }
};

struct Chunk {
    godot::Vector3 chunkPos;
    std::map<DiscreteLandTypeByHeight, LandTypeMeshData> meshes;

    Chunk() = default;

    Chunk(Chunk&&) = default;
    Chunk& operator=(Chunk&&) = default;

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    ~Chunk();

    void initialize(
        const godot::Vector3& position,
        const std::map<DiscreteLandTypeByHeight, i32>& typeCounts
    );
};

class MapManager {
public:
    explicit MapManager(SeparatedMapResult sep)
        : elevations(std::move(sep.discrete)),
          gridWidth(sep.mapResult.width),
          gridHeight(sep.mapResult.height) {}

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

private:
    std::unordered_map<godot::Vector2i, Chunk> loadedChunks;
    std::future<void> futureResult;
    std::unique_ptr<DiscreteLandTypeByHeight[]> elevations;

public:
    Chunk* chunk = nullptr;
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
