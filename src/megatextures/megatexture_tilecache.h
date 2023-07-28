#ifndef __MEGATEXTURE_TILE_CACHE_H__
#define __MEGATEXTURE_TILE_CACHE_H__

#include <ultra64.h>
#include "tile_index.h"

#define MT_TILE_SIZE   (32 * 32 * 2)
#define MT_TILE_WORDS  (MT_TILE_SIZE / sizeof(u64))
#define MT_GFX_SIZE    10

#define MT_TILE_QUEUE_SIZE     64

#define MT_NO_TILE_INDEX       0xFFFF

struct MTTileCacheEntry {
    u16 newerTile;
    u16 olderTile;
    
    u16 nextHashTile;
    u16 hashTableIndex;

    void* romAddress;
};

struct MTTileCache {
    OSPiHandle* piHandle;
    struct MTTileCacheEntry* entries;
    u16* hashTable;
    u64* tileData;
    Gfx* tileLoaders;
    OSMesgQueue tileQueue;
    OSMesg inboundMessages[MT_TILE_QUEUE_SIZE];
    OSIoMesg outboundMessages[MT_TILE_QUEUE_SIZE];
    u16 pendingMessages;
    u16 nextOutboundMessage;
    u16 entryCount;
    u16 hashTableMask;
    u16 oldestUsedTile;
    u16 newestUsedTile;

    // index 0 is the current frame
    // index 1 is the previous frame
    u16 oldestTileFromFrame[2];
};

void mtTileCacheInit(struct MTTileCache* tileCache, int entryCount);
Gfx* mtTileCacheRequestTile(struct MTTileCache* tileCache, struct MTTileIndex* index, int x, int y, int lod);
void mtTileCachePreloadTile(struct MTTileCache* tileCache, struct MTTileIndex* index, int x, int y, int lod);
void mtTileCacheWaitForTiles(struct MTTileCache* tileCache);

#endif