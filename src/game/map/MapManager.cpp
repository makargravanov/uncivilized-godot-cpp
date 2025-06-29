//
// Created by Alex on 27.06.2025.
//

#include "MapManager.h"
#include "../SystemNexus.h"
#include "elevations-creation/DiscreteLandTypeByHeight.h"

f32 MapManager::tileHorizontalOffset   = 173.205078 / 100;
f32 MapManager::oddRowHorizontalOffset = 86.602539 / 100;
f32 MapManager::tileVerticalOffset     = 150.0 / 100;
u8  MapManager::renderDistance         = 1;

void Chunk::initialize(const godot::Vector3 position) {
    chunkPos = position;

    plainMultiMesh->set_use_colors(true);
    plainMultiMesh->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
    plainMultiMesh->set_instance_count(CHUNK_SIZE * CHUNK_SIZE);
    if (const godot::Ref<godot::Resource> plainMeshRes =
            godot::ResourceLoader::get_singleton()->load("res://hexagon_mesh.tres");
        plainMeshRes.is_valid()) {
        plainMultiMesh->set_mesh(plainMeshRes);
        SystemNexus::playScene()->addISM(plainMeshInstance);
    } else {
        godot::print_line("Failed to load plain mesh, using fallback.");
    }

    hillMultiMesh->set_use_colors(true);
    hillMultiMesh->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
    hillMultiMesh->set_instance_count(CHUNK_SIZE * CHUNK_SIZE);
    if (const godot::Ref<godot::Resource> hillMeshRes =
            godot::ResourceLoader::get_singleton()->load("res://hill_hexagon_mesh.tres");
        hillMeshRes.is_valid()) {
        hillMultiMesh->set_mesh(hillMeshRes);
        SystemNexus::playScene()->addISM(hillMeshInstance);
    } else {
        godot::print_line("Failed to load hill mesh, using fallback.");
    }

    mountainMultiMesh->set_use_colors(true);
    mountainMultiMesh->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
    mountainMultiMesh->set_instance_count(CHUNK_SIZE * CHUNK_SIZE);
    if (const godot::Ref<godot::Resource> mountainMeshRes =
            godot::ResourceLoader::get_singleton()->load("res://mountain_hexagon_mesh.tres");
        mountainMeshRes.is_valid()) {
        mountainMultiMesh->set_mesh(mountainMeshRes);
        SystemNexus::playScene()->addISM(mountainMeshInstance);
    } else {
        godot::print_line("Failed to load mountain mesh, using fallback.");
    }
}
Chunk::~Chunk() {
    if (plainMeshInstance) {
        SystemNexus::playScene()->removeISM(plainMeshInstance);
        godot::memdelete(plainMeshInstance);
    }
    if (hillMeshInstance) {
        SystemNexus::playScene()->removeISM(hillMeshInstance);
        godot::memdelete(hillMeshInstance);
    }
    if (mountainMeshInstance) {
        SystemNexus::playScene()->removeISM(mountainMeshInstance);
        godot::memdelete(mountainMeshInstance);
    }
}

void MapManager::unloadChunk(godot::Vector2i chunkPos) {
    if (auto it = loadedChunks.find(chunkPos); it != loadedChunks.end()) {
        loadedChunks.erase(it);
    }
}

void MapManager::loadChunk(godot::Vector2i vec) {
    auto newChunk = Chunk(godot::Vector3(vec.x, 0,vec.y));

    bool flag0 = false;
    bool flag1 = false;
    bool flag2 = false;

    godot::print_line("Loading chunk at: ", vec.x, ", ", vec.y);
    for (i32 localY = 0; localY < CHUNK_SIZE; ++localY) {
        for (i32 localX = 0; localX < CHUNK_SIZE; ++localX) {
            const i32 globalX = vec.x * CHUNK_SIZE + localX;
            const i32 globalY = vec.y * CHUNK_SIZE + localY;
            if (globalX >= 0 && globalX < gridWidth && globalY >= 0 && globalY < gridHeight) {
                godot::Transform3D tileTransform = calculateTileTransform(globalX, globalY);
                const auto type = elevations[globalY * gridWidth + globalX];
                if (type == MOUNTAIN) {
                    flag0 = true;
                    newChunk.mountainMultiMesh->set_instance_transform(localY * CHUNK_SIZE + localX, tileTransform);
                }else if (type == HILL) {
                    flag1 = true;
                    newChunk.hillMultiMesh->set_instance_transform(localY * CHUNK_SIZE + localX, tileTransform);
                }else if (type == PLAIN) {
                    flag2 = true;
                    newChunk.plainMultiMesh->set_instance_transform(localY * CHUNK_SIZE + localX, tileTransform);
                } else {

                }
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
            if (isChunkInRenderDistance(checkChunkPos, godot::Vector2i(playerChunkPos.x, playerChunkPos.z))) {
                chunksToBeVisible.insert(checkChunkPos);
            }
        }
    }

    std::vector<godot::Vector2i> chunksToUnload;
    for (const auto& pair : loadedChunks) {
        if (chunksToBeVisible.find(pair.first) == chunksToBeVisible.end()) {
            chunksToUnload.push_back(pair.first);
        }
    }

    for (const auto& chunkPos : chunksToUnload) {
        unloadChunk(chunkPos);
    }

    for (const auto& chunkPos : chunksToBeVisible) {
        if (loadedChunks.find(chunkPos) == loadedChunks.end()) {
            loadChunk(chunkPos);
        }
    }
}

bool MapManager::isChunkInRenderDistance(const godot::Vector2i& chunkPos, const godot::Vector2i& playerChunkPos) {
    godot::Vector2 diff = godot::Vector2(chunkPos - playerChunkPos);
    return diff.length() <= renderDistance;
}

void MapManager::positionUpdated(const godot::Variant& position) {
    const godot::Vector3 newPosition = position;
    updateVisibleChunks(newPosition);

}
godot::Vector3 MapManager::getChunkPos(const godot::Vector3& worldPos) {
    auto res = godot::Vector3(
                floor(worldPos.x / (tileHorizontalOffset * CHUNK_SIZE)),
                0,
                floor(worldPos.z / (tileVerticalOffset * CHUNK_SIZE))
           );
    return res;
}

