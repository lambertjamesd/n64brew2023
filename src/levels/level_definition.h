#ifndef __LEVEL_LEVEL_DEFINITION_H__
#define __LEVEL_LEVEL_DEFINITION_H__

#include "../megatextures/tile_index.h"

struct LevelDefinition {
    struct MTTileIndex* megatextureIndexes;

    short megatextureIndexcount;
};

#define ADJUST_POINTER_POS(ptr, offset) (void*)((ptr) ? (char*)(ptr) + (offset) : 0)

struct LevelDefinition* levelDefinitionFixPointers(struct LevelDefinition* source, u32 pointerOffset);

#endif