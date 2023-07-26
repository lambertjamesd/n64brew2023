#include "./level_definition.h"

void levelDefinitionFixTileIndexPointers(struct MTTileIndex* tileIndex, u32 pointerOffset, u32 imagePointerOffset) {
    tileIndex->layers = ADJUST_POINTER_POS(tileIndex->layers, pointerOffset);

    for (int i = 0; i < tileIndex->layerCount; ++i) {
        struct MTTileLayer* layer = &tileIndex->layers[i];

        layer->mesh.vertices = ADJUST_POINTER_POS(layer->mesh.vertices, pointerOffset);
        layer->mesh.indices = ADJUST_POINTER_POS(layer->mesh.indices, pointerOffset);
        layer->mesh.tiles = ADJUST_POINTER_POS(layer->mesh.tiles, pointerOffset);

        layer->tileSource = ADJUST_POINTER_POS(layer->tileSource, imagePointerOffset);
    }
}

struct LevelDefinition* levelDefinitionFixPointers(struct LevelDefinition* source, u32 pointerOffset, u32 imagePointerOffset) {
    struct LevelDefinition* result = ADJUST_POINTER_POS(source, pointerOffset);

    result->megatextureIndexes = ADJUST_POINTER_POS(result->megatextureIndexes, pointerOffset);

    for (int i = 0; i < result->megatextureIndexcount; ++i) {
        levelDefinitionFixTileIndexPointers(&result->megatextureIndexes[i], pointerOffset, imagePointerOffset);
    }

    return result;
}