#ifndef __LEVEL_LEVEL_DEFINITION_H__
#define __LEVEL_LEVEL_DEFINITION_H__

#include "../megatextures/tile_index.h"

#include "../math/vector3.h"
#include "../math/plane.h"
#include "../math/box3d.h"

struct CollisionQuad {
    struct Vector3 corner;
    struct Vector3 edgeA;
    struct Vector3 edgeB;

    float edgeALength;
    float edgeBLength;

    struct Plane plane;
    struct Box3D bb;
};

struct LevelDefinition {
    struct MTTileIndex* megatextureIndexes;
    struct CollisionQuad* collisionQuads;

    short megatextureIndexCount;
    short collisionQuadCount;
};

#define ADJUST_POINTER_POS(ptr, offset) (void*)((ptr) ? (char*)(ptr) + (offset) : 0)

struct LevelDefinition* levelDefinitionFixPointers(struct LevelDefinition* source, u32 pointerOffset, u32 imagePointerOffset);

#endif