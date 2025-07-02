//
// Created by Alex on 27.06.2025.
//

#ifndef MAPMANAGER_H
#define MAPMANAGER_H
#include "../../api-classes/PlayScene.h"
#include "../../util/declarations.h"
#include "elevations-creation/LayerSeparator.h"
#include <future>
#include <map>
#include <memory>

#include "godot_cpp/core/print_string.hpp"
#include "godot_cpp/variant/variant.hpp"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

constexpr u8 CHUNK_SIZE = 64;

enum DiscreteLandTypeByHeight : u8;

template <>
struct std::hash<godot::Vector2i> {
    size_t operator()(const godot::Vector2i& point) const noexcept {
        return static_cast<size_t>(point.x) ^ (static_cast<size_t>(point.y) << 16);
    }
};

struct Chunk {
    godot::Vector3 chunkPos;
    godot::MultiMeshInstance3D* plainMeshInstance = nullptr;
    godot::MultiMesh* plainMultiMesh = nullptr;
    godot::MultiMeshInstance3D* hillMeshInstance = nullptr;
    godot::MultiMesh* hillMultiMesh = nullptr;
    godot::MultiMeshInstance3D* mountainMeshInstance = nullptr;
    godot::MultiMesh* mountainMultiMesh              = nullptr;

    explicit Chunk(godot::Vector3 position) {
        plainMeshInstance = memnew(godot::MultiMeshInstance3D);
        plainMultiMesh    = memnew(godot::MultiMesh);
        plainMeshInstance->set_multimesh(plainMultiMesh);

        hillMeshInstance = memnew(godot::MultiMeshInstance3D);
        hillMultiMesh    = memnew(godot::MultiMesh);
        hillMeshInstance->set_multimesh(hillMultiMesh);

        mountainMeshInstance = memnew(godot::MultiMeshInstance3D);
        mountainMultiMesh    = memnew(godot::MultiMesh);
        mountainMeshInstance->set_multimesh(mountainMultiMesh);
        initialize(position);
    }

    void resetMultiMesh(godot::MultiMesh* multimesh);
    void initialize(godot::Vector3 position);

    Chunk(const Chunk& other) = delete;
    Chunk& operator=(const Chunk& other) = delete;

    Chunk(Chunk&& other) noexcept
       : chunkPos(std::move(other.chunkPos)), plainMeshInstance(other.plainMeshInstance),
         plainMultiMesh(other.plainMultiMesh), hillMeshInstance(other.hillMeshInstance),
         hillMultiMesh(other.hillMultiMesh), mountainMeshInstance(other.mountainMeshInstance),
         mountainMultiMesh(other.mountainMultiMesh) {
        other.plainMeshInstance = nullptr;
        other.plainMultiMesh = nullptr;
        other.hillMeshInstance = nullptr;
        other.hillMultiMesh = nullptr;
        other.mountainMeshInstance = nullptr;
        other.mountainMultiMesh = nullptr;
    }

    Chunk& operator=(Chunk&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        chunkPos             = std::move(other.chunkPos);
        plainMeshInstance    = other.plainMeshInstance;
        plainMultiMesh       = other.plainMultiMesh;
        hillMeshInstance     = other.hillMeshInstance;
        hillMultiMesh        = other.hillMultiMesh;
        mountainMeshInstance = other.mountainMeshInstance;
        mountainMultiMesh    = other.mountainMultiMesh;

        other.plainMeshInstance = nullptr;
        other.plainMultiMesh = nullptr;
        other.hillMeshInstance = nullptr;
        other.hillMultiMesh = nullptr;
        other.mountainMeshInstance = nullptr;
        other.mountainMultiMesh = nullptr;

        return *this;
    }

    ~Chunk();
};

class MapManager {

public:
    explicit MapManager(SeparatedMapResult sep) : elevations(std::move(sep.discrete)),
        gridWidth(sep.mapResult.width),
        gridHeight(sep.mapResult.height) {}

    MapManager(const MapManager& other)                = delete;
    MapManager& operator=(const MapManager& other)     = delete;

    MapManager(MapManager&& other) noexcept            = delete;
    MapManager& operator=(MapManager&& other) noexcept = delete;

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
        const f32 xPos     = oddRow ? x * tileHorizontalOffset + oddRowHorizontalOffset : x * tileHorizontalOffset;
        constexpr f32 yPos = 0.0f; // В Godot Y - это вертикаль
        const f32 zPos     = y * tileVerticalOffset;

        return godot::Transform3D(godot::Basis(), // Единичная матрица (без вращения)
            godot::Vector3(xPos, yPos, zPos)
        );
    }
};



#endif //MAPMANAGER_H
