#include "megatexture_tilecache.h"

#include "../util/memory.h"

#define LARGE_PRIME_NUMBER  2147483647

void mtTileCacheInit(struct MTTileCache* tileCache, int entryCount, OSPiHandle* piHandle) {
    tileCache->piHandle = piHandle;
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
    }

    osCreateMesgQueue(&tileCache->tileQueue, &tileCache->messages[0], MT_TILE_QUEUE_SIZE);
    tileCache->pendingMessages = 0;
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

    tileCache->oldestUsedTile = entry->olderTile;
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

    OSIoMesg dmaIoMesgBuf;
    OSMesg dummyMesg;

    dmaIoMesgBuf.hdr.pri      = OS_MESG_PRI_NORMAL;
    dmaIoMesgBuf.hdr.retQueue = &tileCache->tileQueue;
    dmaIoMesgBuf.dramAddr     = (void*)&tileCache->tileData[entryIndex * MT_TILE_SIZE];
    dmaIoMesgBuf.devAddr      = (u32)romAddress;
    dmaIoMesgBuf.size         = MT_TILE_SIZE;

    if (tileCache->pendingMessages == MT_TILE_QUEUE_SIZE) {
        (void)osRecvMesg(&tileCache->tileQueue, &dummyMesg, OS_MESG_BLOCK);
        --tileCache->pendingMessages;
    }

    osEPiStartDma(tileCache->piHandle, &dmaIoMesgBuf, OS_READ);
    ++tileCache->pendingMessages;

    // TODO update gfx
}

Gfx* mtTileCacheRequestTile(struct MTTileCache* tileCache, void* romAddress) {
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

    struct MTTileCacheEntry* entry = &tileCache->entries[entryIndex];
    mtTileCacheRequestFromRom(tileCache, entry, romAddress);
    return &tileCache->tileLoaders[entryIndex * MT_GFX_SIZE];
}

void mtTileCacheWaitForTiles(struct MTTileCache* tileCache) {
    OSMesg dummyMesg;

    while (tileCache->pendingMessages > 0) {
        (void)osRecvMesg(&tileCache->tileQueue, &dummyMesg, OS_MESG_BLOCK);
        --tileCache->pendingMessages;
    }
}