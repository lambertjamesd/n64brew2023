#ifndef __MEGATEXTURE_TILE_INDEX_H__
#define __MEGATEXTURE_TILE_INDEX_H__

#include <ultra64.h>

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

struct MTTileLayer {
    u64* tileSource;
    u8 xTiles;
    u8 yTiles;

    struct MTMeshLayer mesh;
};

struct MTTileIndex {
    struct MTTileLayer* layers;
    u8 layerCount;
};

#endif