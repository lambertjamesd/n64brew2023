#include "megatexture_tilecache.h"

#include "../util/memory.h"

#define LARGE_PRIME_NUMBER  2147483647

void mtTileCacheBuildTileLoader(struct MTTileCache* tileCache, int entryIndex) {
    Gfx* dl = &tileCache->tileLoaders[entryIndex * MT_GFX_SIZE];
    // call this before using the display list
    gDPPipeSync(dl++);
    gDPTileSync(dl++);
    gDPSetTextureImage(dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 1, &tileCache->tileData[entryIndex * MT_TILE_WORDS]);
    gDPSetTile(dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0, G_TX_CLAMP | G_TX_NOMIRROR, 5, 0, G_TX_CLAMP | G_TX_NOMIRROR, 5, 0);
    gDPLoadSync(dl++);
    gDPLoadBlock(dl++, G_TX_LOADTILE, 0, 0, 1023, 256);
    gDPPipeSync(dl++);
    gDPSetTile(dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, 0, 0, G_TX_CLAMP | G_TX_NOMIRROR, 5, 0, G_TX_CLAMP | G_TX_NOMIRROR, 5, 0);
    gDPSetTileSize(dl++, 0, 0, 0, 124, 124);
    gSPEndDisplayList(dl++);
}

void mtTileCacheInit(struct MTTileCache* tileCache, int entryCount) {
    tileCache->piHandle = osCartRomInit();
    tileCache->entries = malloc(sizeof(struct MTTileCacheEntry) * entryCount);
    tileCache->tileData = malloc(MT_TILE_SIZE * entryCount);
    tileCache->tileLoaders = malloc(sizeof(Gfx) * MT_GFX_SIZE * entryCount);
    osInvalDCache((void *)tileCache->tileData, MT_TILE_SIZE * entryCount);
    int hashSize = 1;

    while (hashSize < entryCount) {
        hashSize <<= 1;
    }

    hashSize <<= 1;

    tileCache->hashTable = malloc(sizeof(u16*) * hashSize);

    tileCache->entryCount = entryCount;
    tileCache->hashTableMask = hashSize - 1;

    tileCache->oldestUsedTile = 0;
    tileCache->newestUsedTile = entryCount - 1;

    for (int i = 0; i < hashSize; ++i) {
        tileCache->hashTable[i] = MT_NO_TILE_INDEX;
    }

    for (int i = 0; i < entryCount; ++i) {
        tileCache->entries[i].newerTile = (i == entryCount - 1) ? MT_NO_TILE_INDEX : i + 1;
        tileCache->entries[i].olderTile = (i == 0) ? MT_NO_TILE_INDEX : i - 1;

        tileCache->entries[i].nextHashTile = MT_NO_TILE_INDEX;
        tileCache->entries[i].hashTableIndex = MT_NO_TILE_INDEX;
        tileCache->entries[i].romAddress = NULL;

        mtTileCacheBuildTileLoader(tileCache, i);
    }

    osWritebackDCache(tileCache->tileLoaders, sizeof(Gfx) * MT_GFX_SIZE * entryCount);
    osCreateMesgQueue(&tileCache->tileQueue, &tileCache->inboundMessages[0], MT_TILE_QUEUE_SIZE);
    tileCache->pendingMessages = 0;
    tileCache->nextOutboundMessage = 0;
}

int mtTileCacheRemoveOldestUsedTile(struct MTTileCache* tileCache) {
    int entryIndex = tileCache->oldestUsedTile;

    struct MTTileCacheEntry* entry = &tileCache->entries[entryIndex];

    if (entry->hashTableIndex != MT_NO_TILE_INDEX) {
        int prevIndex = MT_NO_TILE_INDEX;
        int hashEntryIndex = tileCache->hashTable[entry->hashTableIndex];

        while (hashEntryIndex != entryIndex) {
            struct MTTileCacheEntry* hashEntry = &tileCache->entries[hashEntryIndex];
            prevIndex = hashEntryIndex;
            hashEntryIndex = hashEntry->nextHashTile;
        }

        if (prevIndex == MT_NO_TILE_INDEX) {
            tileCache->hashTable[entry->hashTableIndex] = entry->nextHashTile;
        } else {
            struct MTTileCacheEntry* prevTile = &tileCache->entries[hashEntryIndex];
            prevTile->nextHashTile = entry->nextHashTile;
        }

        entry->hashTableIndex = MT_NO_TILE_INDEX;
        entry->nextHashTile = MT_NO_TILE_INDEX;
    }

    tileCache->oldestUsedTile = entry->newerTile;
    struct MTTileCacheEntry* nextTile = &tileCache->entries[entry->newerTile];
    nextTile->olderTile = MT_NO_TILE_INDEX;
    entry->newerTile = MT_NO_TILE_INDEX;

    return entryIndex;
}

void mtTileCacheAdd(struct MTTileCache* tileCache, int entryIndex, int hashIndex) {
    struct MTTileCacheEntry* prevTile = &tileCache->entries[tileCache->newestUsedTile];
    struct MTTileCacheEntry* entry = &tileCache->entries[entryIndex];

    entry->olderTile = tileCache->newestUsedTile;
    entry->newerTile = MT_NO_TILE_INDEX;

    prevTile->newerTile = entryIndex;
    tileCache->newestUsedTile = entryIndex;

    entry->nextHashTile = tileCache->hashTable[hashIndex];
    entry->hashTableIndex = hashIndex;
    tileCache->hashTable[hashIndex] = entryIndex;
}

void mtTileCacheMarkMostRecent(struct MTTileCache* tileCache, int entryIndex) {
    struct MTTileCacheEntry* entry = &tileCache->entries[entryIndex];

    if (entry->newerTile == MT_NO_TILE_INDEX) {
        // already newest tile
        return;
    }

    // remove from linked list
    struct MTTileCacheEntry* newerTile = &tileCache->entries[entry->newerTile];

    if (entry->olderTile == MT_NO_TILE_INDEX) {
        tileCache->oldestUsedTile = entry->newerTile;
        newerTile->olderTile = MT_NO_TILE_INDEX;
    } else {
        struct MTTileCacheEntry* olderTile = &tileCache->entries[entry->olderTile];
        olderTile->newerTile = entry->newerTile;
        newerTile->olderTile = entry->olderTile;
    }

    // add back to the beginning
    struct MTTileCacheEntry* newestTile = &tileCache->entries[tileCache->newestUsedTile];
    newestTile->newerTile = entryIndex;
    entry->olderTile = tileCache->newestUsedTile;
    tileCache->newestUsedTile = entryIndex;
    entry->newerTile = MT_NO_TILE_INDEX;
}

void mtTileCacheRequestFromRom(struct MTTileCache* tileCache, int entryIndex, void* romAddress) {
    struct MTTileCacheEntry* entry = &tileCache->entries[entryIndex];

    entry->romAddress = romAddress;

    if (tileCache->pendingMessages == MT_TILE_QUEUE_SIZE) {
        OSMesg dummyMesg;
        (void)osRecvMesg(&tileCache->tileQueue, &dummyMesg, OS_MESG_BLOCK);
        --tileCache->pendingMessages;
    }

    OSIoMesg* dmaIoMesgBuf = &tileCache->outboundMessages[tileCache->nextOutboundMessage];
    ++tileCache->nextOutboundMessage;

    if (tileCache->nextOutboundMessage == MT_TILE_QUEUE_SIZE) {
        tileCache->nextOutboundMessage = 0;
    }

    dmaIoMesgBuf->hdr.pri      = OS_MESG_PRI_NORMAL;
    dmaIoMesgBuf->hdr.retQueue = &tileCache->tileQueue;
    dmaIoMesgBuf->dramAddr     = (void*)&tileCache->tileData[entryIndex * MT_TILE_WORDS];
    dmaIoMesgBuf->devAddr      = (u32)romAddress;
    dmaIoMesgBuf->size         = MT_TILE_SIZE;

    osEPiStartDma(tileCache->piHandle, dmaIoMesgBuf, OS_READ);
    ++tileCache->pendingMessages;
}

void mtTileCacheFixDisplayList(struct MTTileCache* tileCache, int entryIndex, int x, int y, int lod) {
    Gfx* dl = &tileCache->tileLoaders[entryIndex * MT_GFX_SIZE];

    dl += (MT_GFX_SIZE - 3);

    // multiply to convert tileX to pixel x, bit shift for fixed point texture coordinates
    // x = (x * 32) << 2
    x <<= 7;
    y <<= 7;

    gDPSetTile(dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, 0, 0, G_TX_CLAMP | G_TX_NOMIRROR, 5, lod, G_TX_CLAMP | G_TX_NOMIRROR, 5, lod);
    gDPSetTileSize(dl++, 0, x, y, x + 124, y + 124);

    osWritebackDCache(dl - 2, sizeof(Gfx) * 2);
}

Gfx* mtTileCacheRequestTile(struct MTTileCache* tileCache, void* romAddress, int x, int y, int lod) {
    int hashIndex = (LARGE_PRIME_NUMBER * (u32)romAddress) & tileCache->hashTableMask;

    int entryIndex = tileCache->hashTable[hashIndex];

    while (entryIndex != MT_NO_TILE_INDEX) {
        struct MTTileCacheEntry* entry = &tileCache->entries[entryIndex];

        if (entry->romAddress == romAddress) {
            mtTileCacheMarkMostRecent(tileCache, entryIndex);
            return &tileCache->tileLoaders[entryIndex * MT_GFX_SIZE];
        }

        entryIndex = entry->nextHashTile;
    }

    entryIndex = mtTileCacheRemoveOldestUsedTile(tileCache);
    mtTileCacheAdd(tileCache, entryIndex, hashIndex);

    mtTileCacheRequestFromRom(tileCache, entryIndex, romAddress);
    mtTileCacheFixDisplayList(tileCache, entryIndex, x, y, lod);
    return &tileCache->tileLoaders[entryIndex * MT_GFX_SIZE];
}

void mtTileCacheWaitForTiles(struct MTTileCache* tileCache) {
    OSMesg dummyMesg;

    while (tileCache->pendingMessages > 0) {
        (void)osRecvMesg(&tileCache->tileQueue, &dummyMesg, OS_MESG_BLOCK);
        --tileCache->pendingMessages;
    }
}