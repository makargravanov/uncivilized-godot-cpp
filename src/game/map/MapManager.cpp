//
// Created by Alex on 27.06.2025.
//

#include "MapManager.h"

#include <cmath>
#include "game/SystemNexus.h"

f32 MapManager::tileHorizontalOffset   = 173.205078 / 100;
f32 MapManager::oddRowHorizontalOffset = 86.602539 / 100;
f32 MapManager::tileVerticalOffset     = 150.0 / 100;
u8  MapManager::renderDistance         = 4;


godot::Ref<godot::Resource> LandTypeMeshData::sharedBiomeMaterial;

void LandTypeMeshData::create() {
    instance = GodotPtr(memnew(godot::MultiMeshInstance3D));
    multiMesh.instantiate();
    instance->set_multimesh(multiMesh);
}

bool LandTypeMeshData::initialize(i32 instanceCount, const char* meshPath) const {
    if (!multiMesh.is_valid() || !instance) {
        return false;
    }

    multiMesh->set_use_custom_data(true);
    multiMesh->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
    multiMesh->set_instance_count(instanceCount);

    const godot::Ref<godot::Resource> meshRes =
        godot::ResourceLoader::get_singleton()->load(meshPath);

    if (meshRes.is_valid()) {
        multiMesh->set_mesh(meshRes);
    } else {
        godot::print_line("Failed to load mesh: ", meshPath);
        return false;
    }

    // Shared biome ShaderMaterial — loaded once, applied to every MultiMeshInstance3D
    if (sharedBiomeMaterial.is_null()) {
        sharedBiomeMaterial = godot::ResourceLoader::get_singleton()->load(
            "res://shaders/tile_biome_material.tres");
    }
    if (sharedBiomeMaterial.is_valid()) {
        instance->set_material_override(sharedBiomeMaterial);
    }

    return true;
}

void LandTypeMeshData::addToScene() const {
    if (instance) {
        SystemNexus::playScene()->addISM(instance.get());
    }
}

void LandTypeMeshData::removeFromScene() const {
    if (instance) {
        SystemNexus::playScene()->removeISM(instance.get());
    }
}

Chunk::~Chunk() {
    for (auto& [type, meshData] : meshes) {
        meshData.removeFromScene();
    }
}

void Chunk::initialize(
    const godot::Vector3& position,
    const std::map<ReliefType, i32>& typeCounts
) {
    chunkPos = position;

    for (const auto& [type, count] : typeCounts) {
        if (count <= 0) {
            continue;
        }

        const char* meshPath = getMeshPath(type);

        LandTypeMeshData meshData;
        meshData.create();

        if (meshData.initialize(count, meshPath)) {
            meshData.addToScene();
            meshes.emplace(type, std::move(meshData));
        }
    }
}


void MapManager::unloadChunk(godot::Vector2i chunkPos) {
    if (auto it = loadedChunks.find(chunkPos); it != loadedChunks.end()) {
        loadedChunks.erase(it);
    }
}

void MapManager::loadChunk(godot::Vector2i vec) {
    struct InstanceData {
        godot::Transform3D transform;
        f32 biomeId;
    };
    std::map<ReliefType, std::vector<InstanceData>> tilesByRelief;

    godot::print_line("Loading chunk at: ", vec.x, ", ", vec.y);

    for (i32 localY = 0; localY < CHUNK_SIZE; ++localY) {
        for (i32 localX = 0; localX < CHUNK_SIZE; ++localX) {
            const i32 globalX = vec.x * CHUNK_SIZE + localX;
            const i32 globalY = vec.y * CHUNK_SIZE + localY;

            if (globalX >= 0 && globalX < gridWidth &&
                globalY >= 0 && globalY < gridHeight) {

                const auto& tile = tiles[globalY * gridWidth + globalX];
                godot::Transform3D tileTransform = calculateTileTransform(globalX, globalY);

                tilesByRelief[tile.relief].push_back(
                    {tileTransform, static_cast<f32>(tile.biome)});
            }
        }
    }

    std::map<ReliefType, i32> typeCounts;
    for (const auto& [relief, instances] : tilesByRelief) {
        typeCounts[relief] = static_cast<i32>(instances.size());
        godot::print_line("Relief ", static_cast<int>(relief), ": ", instances.size(), " tiles");
    }

    Chunk newChunk;
    newChunk.initialize(godot::Vector3(vec.x, 0, vec.y), typeCounts);

    for (const auto& [relief, instances] : tilesByRelief) {
        if (auto it = newChunk.meshes.find(relief); it != newChunk.meshes.end()) {
            for (i32 i = 0; i < static_cast<i32>(instances.size()); ++i) {
                it->second.setInstanceTransform(i, instances[i].transform);
                it->second.setInstanceCustomData(i,
                    godot::Color(instances[i].biomeId, 0.0f, 0.0f, 0.0f));
            }
        }
    }

    loadedChunks.emplace(vec, std::move(newChunk));
}

void MapManager::updateVisibleChunks(const godot::Vector3& playerPosition) {
    const godot::Vector3 playerChunkPos = getChunkPos(playerPosition);
    std::set<godot::Vector2i> chunksToBeVisible;

    const i32 checkDistance = renderDistance;
    for (i32 dx = -checkDistance; dx <= checkDistance; ++dx) {
        for (i32 dy = -checkDistance; dy <= checkDistance; ++dy) {
            godot::Vector2i checkChunkPos(
                playerChunkPos.x + dx,
                playerChunkPos.z + dy
            );
            if (isChunkInRenderDistance(checkChunkPos,
                godot::Vector2i(playerChunkPos.x, playerChunkPos.z))) {
                chunksToBeVisible.insert(checkChunkPos);
            }
        }
    }

    std::vector<godot::Vector2i> chunksToUnload;

    for (const auto& chunkPos : chunksToBeVisible) {
        if (loadedChunks.find(chunkPos) == loadedChunks.end()) {
            loadChunk(chunkPos);
        }
    }

    for (const auto& pair : loadedChunks) {
        if (chunksToBeVisible.find(pair.first) == chunksToBeVisible.end()) {
            chunksToUnload.push_back(pair.first);
        }
    }

    for (const auto& chunkPos : chunksToUnload) {
        unloadChunk(chunkPos);
    }
}

bool MapManager::isChunkInRenderDistance(
    const godot::Vector2i& chunkPos,
    const godot::Vector2i& playerChunkPos
) {
    godot::Vector2 diff = godot::Vector2(chunkPos - playerChunkPos);
    return diff.length() <= renderDistance;
}

void MapManager::positionUpdated(const godot::Variant& position) {
    const godot::Vector3 newPosition = position;
    updateVisibleChunks(newPosition);
}

godot::Vector3 MapManager::getChunkPos(const godot::Vector3& worldPos) {
    return godot::Vector3(
        std::floor(worldPos.x / (tileHorizontalOffset * CHUNK_SIZE)),
        0,
        std::floor(worldPos.z / (tileVerticalOffset * CHUNK_SIZE))
    );
}

