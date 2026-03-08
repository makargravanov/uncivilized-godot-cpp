//
// Created by Alex on 27.06.2025.
//

#include "MapManager.h"

#include <cmath>

#include "game/climate/TemperaturePass.h"
#include "game/SystemNexus.h"
#include "BiomeClassifier.h"

namespace {

constexpr i32 CUSTOM_DATA_FLOAT_COUNT = 4;
constexpr f32 BUFFER_COMPARE_EPSILON = 1e-6f;

void refreshMeshOverlay(LandTypeMeshData& meshData, const ViewMode mode, const OverlayFunc& overlayFunc) {
    if (meshData.material.is_valid() && meshData.lastAppliedViewMode != mode) {
        meshData.material->set_shader_parameter("view_mode", static_cast<int>(mode));
        meshData.lastAppliedViewMode = mode;
    }

    for (i32 index = 0; index < static_cast<i32>(meshData.tileIndices.size()); ++index) {
        const f32 overlayValue = (mode != VIEW_NORMAL && overlayFunc)
            ? overlayFunc(meshData.tileIndices[index])
            : 0.0f;
        meshData.setCachedCustomDataAlpha(index, overlayValue);
    }

    meshData.applyBufferCache();
}

void refreshMeshBiomeData(
    LandTypeMeshData& meshData,
    TileData* tiles,
    const ViewMode mode,
    const OverlayFunc& overlayFunc) {
    if (!tiles) {
        return;
    }

    for (i32 index = 0; index < static_cast<i32>(meshData.tileIndices.size()); ++index) {
        const i32 tileIndex = meshData.tileIndices[index];
        const TileData& tile = tiles[tileIndex];
        const f32 overlayValue = (mode != VIEW_NORMAL && overlayFunc)
            ? overlayFunc(tileIndex)
            : 0.0f;
        meshData.setInstanceCustomData(index,
            godot::Color(
                static_cast<f32>(tile.biome),
                static_cast<f32>(tile.river_edges),
                static_cast<f32>(tile.features),
                overlayValue));
    }

    meshData.captureBufferCache();
}

} // namespace

f32 MapManager::tileHorizontalOffset   = 173.205078 / 100;
f32 MapManager::oddRowHorizontalOffset = 86.602539 / 100;
f32 MapManager::tileVerticalOffset     = 150.0 / 100;
u8  MapManager::renderDistance         = 4;


void LandTypeMeshData::create() {
    instance = GodotPtr(memnew(godot::MultiMeshInstance3D));
    multiMesh.instantiate();
    instance->set_multimesh(multiMesh);
}

bool LandTypeMeshData::initialize(i32 instanceCount, const char* meshPath, const char* materialPath) {
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

    // Per-relief ShaderMaterial (ocean / land / mountain)
    const godot::Ref<godot::Resource> matRes =
        godot::ResourceLoader::get_singleton()->load(materialPath);
    if (matRes.is_valid()) {
        instance->set_material_override(matRes);
        material = matRes;
    } else {
        godot::print_line("Failed to load material: ", materialPath);
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

void LandTypeMeshData::captureBufferCache() {
    instanceBufferCache = godot::PackedFloat32Array();
    instanceFloatStride = 0;
    customDataOffset = -1;
    bufferCacheDirty = false;

    if (!multiMesh.is_valid()) {
        return;
    }

    const i32 instanceCount = multiMesh->get_instance_count();
    if (instanceCount <= 0) {
        return;
    }

    instanceBufferCache = multiMesh->get_buffer();
    const i32 bufferSize = instanceBufferCache.size();
    if (bufferSize <= 0 || bufferSize % instanceCount != 0) {
        instanceBufferCache = godot::PackedFloat32Array();
        return;
    }

    instanceFloatStride = bufferSize / instanceCount;
    if (multiMesh->is_using_custom_data() && instanceFloatStride >= CUSTOM_DATA_FLOAT_COUNT) {
        customDataOffset = instanceFloatStride - CUSTOM_DATA_FLOAT_COUNT;
    }
}

void LandTypeMeshData::setCachedCustomDataAlpha(const i32 index, const f32 alpha) {
    if (customDataOffset < 0 || instanceFloatStride <= 0 || index < 0) {
        return;
    }

    const i32 baseOffset = index * instanceFloatStride + customDataOffset;
    if (baseOffset + 3 >= instanceBufferCache.size()) {
        return;
    }

    f32* bufferWrite = instanceBufferCache.ptrw();
    if (std::abs(bufferWrite[baseOffset + 3] - alpha) <= BUFFER_COMPARE_EPSILON) {
        return;
    }

    bufferWrite[baseOffset + 3] = alpha;
    bufferCacheDirty = true;
}

void LandTypeMeshData::applyBufferCache() {
    if (!bufferCacheDirty || !multiMesh.is_valid() || instanceBufferCache.size() == 0) {
        return;
    }

    multiMesh->set_buffer(instanceBufferCache);
    bufferCacheDirty = false;
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

        const char* meshPath     = getMeshPath(type);
        const char* materialPath = getMaterialPath(type);

        LandTypeMeshData meshData;
        meshData.create();

        if (meshData.initialize(count, meshPath, materialPath)) {
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
        f32 riverEdges;
        f32 features;
        i32 tileIndex;
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
                const i32 tileIndex = globalY * gridWidth + globalX;

                tilesByRelief[tile.relief].push_back({
                    tileTransform,
                    static_cast<f32>(tile.biome),
                    static_cast<f32>(tile.river_edges),
                    static_cast<f32>(tile.features),
                    tileIndex
                });
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
            it->second.tileIndices.reserve(instances.size());
            for (i32 i = 0; i < static_cast<i32>(instances.size()); ++i) {
                it->second.setInstanceTransform(i, instances[i].transform);

                f32 overlayVal = 0.0f;
                if (currentViewMode != VIEW_NORMAL && currentOverlayFunc) {
                    overlayVal = currentOverlayFunc(instances[i].tileIndex);
                }

                it->second.setInstanceCustomData(i,
                    godot::Color(instances[i].biomeId,
                                 instances[i].riverEdges,
                                 instances[i].features,
                                 overlayVal));
                it->second.tileIndices.push_back(instances[i].tileIndex);
            }

            it->second.captureBufferCache();

            // Apply current view_mode uniform.
            if (it->second.material.is_valid()) {
                it->second.material->set_shader_parameter("view_mode", static_cast<int>(currentViewMode));
                it->second.lastAppliedViewMode = currentViewMode;
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

void MapManager::setViewMode(ViewMode mode, const OverlayFunc& overlayFunc) {
    currentViewMode = mode;
    currentOverlayFunc = overlayFunc;

    for (auto& [chunkPos, chunk] : loadedChunks) {
        for (auto& [relief, meshData] : chunk.meshes) {
            refreshMeshOverlay(meshData, mode, overlayFunc);
        }
    }
}

void MapManager::updateTemperatureSnapshot(const ClimateState& climateState) {
    TemperaturePass::publishToTiles(climateState, tiles);

    if (currentViewMode == VIEW_TEMPERATURE && currentOverlayFunc) {
        setViewMode(currentViewMode, currentOverlayFunc);
    }
}

bool MapManager::updateBiomeSnapshot(const ClimateState& climateState) {
    if (!tiles || !BiomeClassifier::classifyFromClimate(climateState, tiles.get(), gridWidth, gridHeight)) {
        return false;
    }

    for (auto& [chunkPos, chunk] : loadedChunks) {
        for (auto& [relief, meshData] : chunk.meshes) {
            refreshMeshBiomeData(meshData, tiles.get(), currentViewMode, currentOverlayFunc);
        }
    }

    return true;
}

