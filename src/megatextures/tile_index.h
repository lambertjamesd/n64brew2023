#ifndef __MEGATEXTURE_TILE_INDEX_H__
#define __MEGATEXTURE_TILE_INDEX_H__

#include <ultra64.h>
#include "../math/vector2.h"
#include "../math/box3d.h"
#include "../math/vector3.h"

struct MTMeshTile {
    u16 startVertex;
    u16 startIndex;
    u8 indexCount;
    u8 vertexCount;
};

struct MTMeshLayer {
    Vtx* vertices;
    u8* indices;
    struct MTMeshTile* tiles;

    u8 minTileX;
    u8 minTileY;
    u8 maxTileX;
    u8 maxTileY;
};

struct MTUVBasis {
    struct Vector3 uvOrigin;
    struct Vector3 uvRight;
    struct Vector3 uvUp;
    struct Vector3 normal;
};

struct MTImageLayer {
    u64* tileSource;
    u8 xTiles;
    u8 yTiles;
    u8 isAlwaysLoaded;
};

struct MTTileIndex {
    struct MTMeshLayer* meshLayers;
    struct MTImageLayer* imageLayers;
    u8 layerCount;
    struct Box3D boundingBox;
    struct MTUVBasis uvBasis;
    struct Vector2 minUv;
    struct Vector2 maxUv;
    float worldPixelSize;
    s16 sortGroup;
};

#endif