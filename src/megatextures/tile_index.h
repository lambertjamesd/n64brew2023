#ifndef __MEGATEXTURE_TILE_INDEX_H__
#define __MEGATEXTURE_TILE_INDEX_H__

#include <ultra64.h>
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
    u8 tileXBits;
};

struct MTTileLayer {
    u64* tileSource;
    u8 xTiles;
    u8 yTiles;
    u8 lod;

    struct MTMeshLayer mesh;

    float worldPixelSize;
};

struct MTUVBasis {
    struct Vector3 uvOrigin;
    struct Vector3 uvRight;
    struct Vector3 uvUp;
    struct Vector3 normal;
};

struct MTTileIndex {
    struct MTTileLayer* layers;
    u8 layerCount;
    struct Box3D boundingBox;
    struct MTUVBasis uvBasis;
    float worldPixelSize;
};

#endif