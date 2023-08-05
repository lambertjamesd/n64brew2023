#include "./level_definition.h"

void levelDefinitionFixTileIndexPointers(struct MTTileIndex* tileIndex, u32 pointerOffset, u32 imagePointerOffset) {
    tileIndex->meshLayers = ADJUST_POINTER_POS(tileIndex->meshLayers, pointerOffset);

    for (int i = 0; i < tileIndex->layerCount; ++i) {
        struct MTMeshLayer* layer = &tileIndex->meshLayers[i];

        layer->vertices = ADJUST_POINTER_POS(layer->vertices, pointerOffset);
        layer->indices = ADJUST_POINTER_POS(layer->indices, pointerOffset);
        layer->tiles = ADJUST_POINTER_POS(layer->tiles, pointerOffset);

    }


    tileIndex->imageLayers = ADJUST_POINTER_POS(tileIndex->imageLayers, pointerOffset);

    for (int i = 0; i < tileIndex->layerCount; ++i) {
        struct MTImageLayer* layer = &tileIndex->imageLayers[i];
        
        layer->tileSource = ADJUST_POINTER_POS(layer->tileSource, imagePointerOffset);
    }
}

struct LevelDefinition* levelDefinitionFixPointers(struct LevelDefinition* source, u32 pointerOffset, u32 imagePointerOffset) {
    struct LevelDefinition* result = ADJUST_POINTER_POS(source, pointerOffset);

    result->megatextureIndexes = ADJUST_POINTER_POS(result->megatextureIndexes, pointerOffset);

    for (int i = 0; i < result->megatextureIndexCount; ++i) {
        levelDefinitionFixTileIndexPointers(&result->megatextureIndexes[i], pointerOffset, imagePointerOffset);
    }

    result->collisionQuads = ADJUST_POINTER_POS(result->collisionQuads, pointerOffset);

    return result;
}