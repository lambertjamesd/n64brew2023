#include "./level_definition.h"

struct MTTileIndex* levelDefinitionFixTileIndexPointers(struct MTTileIndex* tileIndex, u32 pointerOffset) {
    struct MTTileIndex* result = ADJUST_POINTER_POS(tileIndex, pointerOffset);

    result->layers = ADJUST_POINTER_POS(result->layers, pointerOffset);

    for (int i = 0; i < result->layerCount; ++i) {
        struct MTTileLayer* layer = &result->layers[i];

        layer->mesh.vertices = ADJUST_POINTER_POS(layer->mesh.vertices, pointerOffset);
        layer->mesh.indices = ADJUST_POINTER_POS(layer->mesh.indices, pointerOffset);
        layer->mesh.tiles = ADJUST_POINTER_POS(layer->mesh.tiles, pointerOffset);
    }

    return result;
}

struct LevelDefinition* levelDefinitionFixPointers(struct LevelDefinition* source, u32 pointerOffset) {
    struct LevelDefinition* result = ADJUST_POINTER_POS(source, pointerOffset);

    result->megatextureIndexes = levelDefinitionFixTileIndexPointers(result->megatextureIndexes, pointerOffset);

    return result;
}