#include "megatexture_renderer.h"

#define MAX_VERTEX_CACHE_SIZE   32

#include "./megatexture_culling_loop.h"
#include "../math/mathf.h"
#include <math.h>
#include "../graphics/graphics.h"
#include "../util/memory.h"

#define MT_MAX_LOD              5
#define MT_MIP_SAMPLE_COUNT     3
#define MT_LOG_INV_2 1.442695041

#define MT_MAX_TOTAL_TILE_REQUESTS  500

#define MT_MIN_TOTAL_TILE_REQUESTS  400
#define MT_MAX_TILE_UNDER_LOADED    64

#define MIN_PIXEL_AREA          0.000015259

#define MT_LOD_BIAS_START       1.5f
#define MT_LOD_BIAS_STEP        (1.0f / 32.0f)
#define MT_LOD_BIAS_FAIL_STEP   (1.0f / 2.0f)

float gMtLodBias = 1.5f;
float gMtMinLoadBias = 1.0f;

float mtCalculateMipLevel(float pixelArea) {
    if (pixelArea < MIN_PIXEL_AREA) {
        return 10.0f;
    }

    return logf(1.0f / (pixelArea)) * (0.5f * MT_LOG_INV_2) + gMtLodBias;
}

float mtScreenSpace(struct CameraMatrixInfo* cameraInfo, struct Vector2* cameraSpacePoint) {
    return (gScreenHeight / 2.0f) * cameraInfo->cotFov * cameraSpacePoint->x / cameraSpacePoint->y;
}

void mtUvBasisTransform(struct Vector3* axis, struct Vector3* right, struct Vector3* up, struct Vector2* result) {
    result->x = vector3Dot(right, axis);
    result->y = vector3Dot(up, axis);
}

void megatextureDetermineMipLevels(struct MTUVBasis* uvBasis, float worldPixelWidth, struct MTCullingLoop* cullingLoop, struct CameraMatrixInfo* cameraInfo, int mipPlaneCount, float* mipPlanesOut, int* minLodOut, int* maxLodOut) {
    struct Vector2 uvSpaceCameraDirection;

    uvSpaceCameraDirection.x = vector3Dot(&uvBasis->uvRight, &cameraInfo->forwardVector);
    uvSpaceCameraDirection.y = vector3Dot(&uvBasis->uvUp, &cameraInfo->forwardVector);

    struct Vector3 cameraOffset;
    vector3Sub(&uvBasis->uvOrigin, &cameraInfo->cameraPosition, &cameraOffset);

    if (fabsf(uvSpaceCameraDirection.x) < 0.000001f && fabsf(uvSpaceCameraDirection.y) < 0.000001f) {
        // camera is looking straight at a plane
        // just use a single mip level
        struct Vector2 cameraSpacePos;
        cameraSpacePos.x = worldPixelWidth;
        cameraSpacePos.y = vector3Dot(&cameraOffset, &cameraInfo->forwardVector);
        float sideSize = mtScreenSpace(cameraInfo, &cameraSpacePos);
        float pixelArea = sideSize * sideSize;
        float mipLevel = mtCalculateMipLevel(pixelArea);
        // not needed but helpful for debugging
        mipPlanesOut[0] = cameraSpacePos.y;

        *minLodOut = (int)floorf(clampf(mipLevel, 0.0f, (float)mipPlaneCount));
        *maxLodOut = *minLodOut;
        return;
    }

    struct Vector3 cameraTangent;
    vector3ProjectPlane(&uvBasis->normal, &cameraInfo->forwardVector, &cameraTangent);
    vector3Normalize(&cameraTangent, &cameraTangent);

    struct Vector2 cameraRight;
    struct Vector2 cameraUp;
    struct Vector2 cameraOrigin;
    mtUvBasisTransform(&uvBasis->uvRight, &cameraTangent, &cameraInfo->forwardVector, &cameraRight);
    mtUvBasisTransform(&uvBasis->uvUp, &cameraTangent, &cameraInfo->forwardVector, &cameraUp);
    mtUvBasisTransform(&cameraOffset, &cameraTangent, &cameraInfo->forwardVector, &cameraOrigin);

    float distance[MT_MIP_SAMPLE_COUNT];
    float mipLevel[MT_MIP_SAMPLE_COUNT];

    float lerp = 0.0f;

    struct Vector2 furthestPoint;
    mtCullingLoopFurthestPoint(cullingLoop, &uvSpaceCameraDirection, &furthestPoint);

    struct Vector2 closestPoint;
    vector2Negate(&uvSpaceCameraDirection, &uvSpaceCameraDirection);
    mtCullingLoopFurthestPoint(cullingLoop, &uvSpaceCameraDirection, &closestPoint);

    struct Vector2 texelOffset;
    vector2Sub(&furthestPoint, &closestPoint, &texelOffset);
    vector2Normalize(&texelOffset, &texelOffset);
    vector2Scale(&texelOffset, worldPixelWidth, &texelOffset);

    for (int i = 0; i < MT_MIP_SAMPLE_COUNT; ++i) {
        struct Vector2 uvSpacePos;
        vector2Lerp(&closestPoint, &furthestPoint, lerp, &uvSpacePos);

        struct Vector2 cameraSpacePos;
        cameraSpacePos.x = cameraOrigin.x + uvSpacePos.x * cameraRight.x + uvSpacePos.y * cameraUp.x;
        cameraSpacePos.y = cameraOrigin.y + uvSpacePos.x * cameraRight.y + uvSpacePos.y * cameraUp.y;

        float crossSize = mtScreenSpace(cameraInfo, &cameraSpacePos);

        vector2Add(&uvSpacePos, &texelOffset, &uvSpacePos);
        cameraSpacePos.x = cameraOrigin.x + uvSpacePos.x * cameraRight.x + uvSpacePos.y * cameraUp.x;
        cameraSpacePos.y = cameraOrigin.y + uvSpacePos.x * cameraRight.y + uvSpacePos.y * cameraUp.y;

        crossSize = fabsf(crossSize - mtScreenSpace(cameraInfo, &cameraSpacePos));

        cameraSpacePos.x = worldPixelWidth;
        float pixelArea = mtScreenSpace(cameraInfo, &cameraSpacePos) * crossSize;

        distance[i] = cameraSpacePos.y;
        mipLevel[i] = mtCalculateMipLevel(pixelArea);

        lerp += (1.0f / (MT_MIP_SAMPLE_COUNT - 1));
    }

    int currentMipIndex = 0;

    *minLodOut = 0;
    *maxLodOut = 0;

    for (int i = 0; i < mipPlaneCount; ++i) {
        while (currentMipIndex < MT_MIP_SAMPLE_COUNT && (float)i > mipLevel[currentMipIndex]) {
            ++currentMipIndex;
        }

        if (currentMipIndex == MT_MIP_SAMPLE_COUNT) {
            // clipping plane is past the end of the polygon
            mipPlanesOut[i] = distance[MT_MIP_SAMPLE_COUNT - 1] + 1.0f;
            continue;
        }

        if (currentMipIndex == 0) {
            // clipping plane is in front of the polygon
            mipPlanesOut[i] = distance[0] - 1.0f;
            // skip the current mip level
            *minLodOut = i + 1;
            *maxLodOut = i + 1;
            continue;
        }

        float lerp = mathfInvLerp(mipLevel[currentMipIndex - 1], mipLevel[currentMipIndex], (float)i);
        mipPlanesOut[i] = mathfLerp(distance[currentMipIndex - 1], distance[currentMipIndex], lerp);
        *maxLodOut = i;
    }
}

void megatextureRenderRow(struct MTTileCache* tileCache, struct MTTileIndex* index, int layerIndex, int row, int minX, int maxX, struct RenderState* renderState) {
    struct MTMeshLayer* meshLayer = &index->meshLayers[layerIndex];
    int currentVertexCount = 0;

    Gfx* vertexCopyCommand = renderState->dl++;

    struct MTMeshTile* tile = &meshLayer->tiles[((row - meshLayer->minTileY) * (meshLayer->maxTileX - meshLayer->minTileX)) + (minX - meshLayer->minTileX)];
    int startVertex = tile->startVertex;

    for (int x = minX; x < maxX; ++x, ++tile) {
        if (tile->indexCount == 0) {
            continue;
        }

        int startIndex = tile->startVertex - startVertex;

        if (tile->vertexCount + startIndex > MAX_VERTEX_CACHE_SIZE) {
            if (currentVertexCount == 0) {
                continue;
            }

            // retroactively update the vertex command
            gSPVertex(vertexCopyCommand, &meshLayer->vertices[startVertex], currentVertexCount, 0);
            vertexCopyCommand = renderState->dl++;
            startVertex = tile->startVertex;
            startIndex = 0;
        }

        currentVertexCount = startIndex + tile->vertexCount;

        Gfx* tileRequest = mtTileCacheRequestTile(tileCache, index, x, row, layerIndex);
        gSPDisplayList(renderState->dl++, tileRequest)

        u8* indices = &meshLayer->indices[tile->startIndex];
        u8* indexEnd = indices + tile->indexCount;

        for (; indices + 5 < indexEnd; indices += 6) {
            gSP2Triangles(
                renderState->dl++, 
                indices[0] + startIndex, indices[1] + startIndex, indices[2] + startIndex, 0,
                indices[3] + startIndex, indices[4] + startIndex, indices[5] + startIndex, 0
            );
        }

        if (indices + 2 < indexEnd) {
            gSP1Triangle(renderState->dl++, indices[0] + startIndex, indices[1] + startIndex, indices[2] + startIndex, 0);
        }
    }

    if (currentVertexCount) {
        // retroactively update the vertex command
        gSPVertex(vertexCopyCommand, &meshLayer->vertices[startVertex], currentVertexCount, 0);
    } else {
        // if there are no vertices used then unallocate the gSPVertex command
        renderState->dl = vertexCopyCommand;
    }
}

int megatextureRenderLayer(struct MTTileCache* tileCache, struct MTTileIndex* index, int layerIndex, struct MTCullingLoop* currentLoop, float nearPlane, float farPlane, struct CameraMatrixInfo* cameraInfo, struct RenderState* renderState) {
    if (currentLoop->loopSize == 0) {
        // the plane is entirely outside the view
        return 1;
    }

    nearPlane *= SCENE_SCALE;
    farPlane *= SCENE_SCALE;

    cameraInfo->projectionMatrix[2][2] = (nearPlane + farPlane) / (nearPlane - farPlane);
    cameraInfo->projectionMatrix[3][2] = (2 * nearPlane * farPlane) / (nearPlane - farPlane);

    int leftIndex = mtCullingLoopTopIndex(currentLoop);
    int rightIndex = leftIndex;

    float lastLeftBoundary = currentLoop->loop[leftIndex].x;
    float lastRightBoundary = lastLeftBoundary;

    struct MTImageLayer* imageLayer = &index->imageLayers[layerIndex];
    struct MTMeshLayer* meshLayer = &index->meshLayers[layerIndex];
    float tileStep = 1.0f / imageLayer->yTiles;
    float nextBoundary = tileStep * (meshLayer->minTileY + 1);

    if (meshLayer->minTileY) {
        // update indices and last boundaries
        mtCullingLoopFindExtent(currentLoop, &leftIndex, &lastLeftBoundary, meshLayer->minTileY * tileStep, 1);
        mtCullingLoopFindExtent(currentLoop, &rightIndex, &lastRightBoundary, meshLayer->minTileY * tileStep, -1);
    }

    for (int row = meshLayer->minTileY; row < meshLayer->maxTileY; ++row, nextBoundary += tileStep) {
        float minX = mtCullingLoopFindExtent(currentLoop, &leftIndex, &lastLeftBoundary, nextBoundary, 1);
        float maxX = mtCullingLoopFindExtent(currentLoop, &rightIndex, &lastRightBoundary, nextBoundary, -1);

        if (minX == maxX) {
            // empty row
            continue;
        }


        Mtx* projection = renderStateRequestMatrices(renderState, 1);   

        if (!projection) {
            return 0;
        }

        guMtxF2L(cameraInfo->projectionMatrix, projection);
        gSPMatrix(renderState->dl++, projection, G_MTX_LOAD | G_MTX_PROJECTION | G_MTX_NOPUSH);
        gSPMatrix(renderState->dl++, cameraInfo->viewMtx, G_MTX_MUL | G_MTX_PROJECTION | G_MTX_NOPUSH);

        megatextureRenderRow(
            tileCache, 
            index,
            layerIndex, 
            row, 
            MAX(meshLayer->minTileX, (int)floorf(minX * imageLayer->xTiles)), 
            MIN(meshLayer->maxTileX, (int)ceilf(maxX * imageLayer->xTiles)), 
            renderState
        );
    }

    return 1;
}

int mtIsBackFacing(struct CameraMatrixInfo* cameraInfo, struct MTUVBasis* basis) {
    struct Vector3 offset;
    vector3Sub(&basis->uvOrigin, &cameraInfo->cameraPosition, &offset);
    return vector3Dot(&offset, &basis->normal) >= 0.0f;
}

int megatextureRender(struct MTTileCache* tileCache, struct MTTileIndex* index, struct CameraMatrixInfo* cameraInfo, struct RenderState* renderState) {
    if (mtIsBackFacing(cameraInfo, &index->uvBasis)) {
        return 1;
    }

    if (isOutsideFrustrum(&cameraInfo->cullingInformation, &index->boundingBox)) {
        return 1;
    }

    struct MTCullingLoop cullingLoop;

    mtCullingLoopInit(&cullingLoop, &index->minUv, &index->maxUv);
    mtCullingLoopClip(&cullingLoop, &index->uvBasis, &cameraInfo->cullingInformation);

    if (cullingLoop.loopSize == 0) {
        // the plane is entirely outside the view
        return 1;
    }

    float clipingPlaneDistances[MT_MAX_LOD];
    int minLod;
    int maxLod;
    megatextureDetermineMipLevels(
        &index->uvBasis, 
        index->worldPixelSize, 
        &cullingLoop, 
        cameraInfo, 
        index->layerCount - 1, 
        clipingPlaneDistances,
        &minLod,
        &maxLod
    );

    float prevPlane = cameraInfo->nearPlane;

    for (int layerIndex = minLod; layerIndex <= maxLod; ++layerIndex) {
        if (layerIndex == maxLod) {
            return megatextureRenderLayer(tileCache, index, layerIndex, &cullingLoop, prevPlane, cameraInfo->farPlane, cameraInfo, renderState);
        }

        float clippingPlaneDistance = clipingPlaneDistances[layerIndex];
        
        struct Plane mipClippingPlane;
        mipClippingPlane.normal = cameraInfo->forwardVector;
        mipClippingPlane.d = -(vector3Dot(&mipClippingPlane.normal, &cameraInfo->cameraPosition) + clippingPlaneDistance);

        struct Plane2 clippingPlane;
        mtProjectClippingPlane(&mipClippingPlane, &index->uvBasis, &clippingPlane);

        struct MTCullingLoop currentLoop;
        mtCullingLoopSplit(&cullingLoop, &clippingPlane, &currentLoop);
        if (!megatextureRenderLayer(tileCache, index, layerIndex, &currentLoop, prevPlane, clippingPlaneDistance, cameraInfo, renderState)) {
            return 0;
        }

        prevPlane = clippingPlaneDistance;
    }

    return 1;
}

void megatexturePreload(struct MTTileCache* tileCache, struct MTTileIndex* index, int minTileAxisTileCount) {
    for (int layerIndex = 0; layerIndex < index->layerCount; ++layerIndex) {
        struct MTImageLayer* layer = &index->imageLayers[layerIndex];

        if (layer->maxTileAxisTileCount > minTileAxisTileCount) {
            continue;
        }

        for (int y = 0; y < layer->yTiles; ++y) {
            for (int x = 0; x < layer->xTiles; ++x) {
                mtTileCachePreloadTile(tileCache, index, x, y, layerIndex);
            }
        }
    }
}

void megatextureRenderStart(struct MTTileCache* tileCache) {
    tileCache->oldestTileFromFrame[1] = tileCache->oldestTileFromFrame[0];
    tileCache->oldestTileFromFrame[0] = MT_NO_TILE_INDEX;
    tileCache->tilesRequestedFromCart = 0;
    tileCache->totalTileRequests = 0;
    tileCache->overflowRequestCount = 0;

    for (int i = 0; i < 6; ++i) {
        tileCache->tileRequests[i] = 0;
    }
}

int megatexturesDoesHaveExtraSpace(struct MTTileCache* tileCache) {
    int extraCount = 0;
    
    int entryIndex = tileCache->oldestUsedTile;

    while (entryIndex != tileCache->oldestTileFromFrame[1]) {
        // this tile was already used this frame and
        // there are no more availible tiles
        ++extraCount;

        if (extraCount == MT_MAX_TILE_UNDER_LOADED) {
            return 1;
        }
        
        entryIndex = tileCache->entries[entryIndex].newerTile;
    }


    return 0;
}

void megatextureRenderEnd(struct MTTileCache* tileCache, int success) {
    mtTileCacheWaitForTiles(tileCache);

    if (!success) {
        gMtLodBias += MT_LOD_BIAS_FAIL_STEP;
        return;
    }

    if (tileCache->overflowRequestCount || tileCache->totalTileRequests > MT_MAX_TOTAL_TILE_REQUESTS) {
        gMtLodBias += MT_LOD_BIAS_STEP;
    } else if (megatexturesDoesHaveExtraSpace(tileCache) && tileCache->totalTileRequests < MT_MIN_TOTAL_TILE_REQUESTS) {
        gMtLodBias -= MT_LOD_BIAS_STEP * 0.25;

        if (gMtLodBias < gMtMinLoadBias) {
            gMtLodBias = gMtMinLoadBias;
        }
    }
}

struct SortInfo {
    float sortKey;
    int index;
};

void megatexturesSort(struct SortInfo* values, struct SortInfo* tmp, int min, int max) {
    if (min + 1 >= max) {
        return;
    }

    int mid = (min + max) / 2;

    megatexturesSort(values, tmp, min, mid);
    megatexturesSort(values, tmp, mid, max);

    int srcA = min;
    int srcB = mid;

    int write = min;

    while (srcA < mid || srcB < max) {
        if (srcB >= max || (srcA < mid && values[srcA].sortKey < values[srcB].sortKey)) {
            tmp[write++] = values[srcA++];
        } else {
            tmp[write++] = values[srcB++];
        }
    }

    for (int i = min; i < max; ++i) {
        values[i] = tmp[i];
    }
}

#define GROUP_SORT_OFFSET   1000.0f

int megatexturesRenderAll(struct MTTileCache* tileCache, struct MTTileIndex* index, int count, struct CameraMatrixInfo* cameraInfo, struct RenderState* renderState) {
    megatextureRenderStart(tileCache);

    int currentFace = 0;

    while (currentFace < count && index[currentFace].sortGroup < 0) {
        if (!megatextureRender(tileCache, &index[currentFace], cameraInfo, renderState)) {
            megatextureRenderEnd(tileCache, 0);
            return 0;
        }

        ++currentFace;
    }

    int sortedFaceCount = count - currentFace;
    struct SortInfo* sortInfo = stackMalloc(sizeof(struct SortInfo) * sortedFaceCount);
    int currentSortFace = 0;

    while (currentFace < count) {
        struct Vector3 furthestPoint;
        box3DSupportFunction(&index[currentFace].boundingBox, &cameraInfo->forwardVector, &furthestPoint);
        furthestPoint.y = 0.0f;
        sortInfo[currentSortFace].sortKey = index[currentFace].sortGroup * GROUP_SORT_OFFSET - vector3Dot(&furthestPoint, &cameraInfo->forwardVector);
        sortInfo[currentSortFace].index = currentFace;
        ++currentFace;
        ++currentSortFace;
    }

    struct SortInfo* tmpMemory = stackMalloc(sizeof(struct SortInfo) * sortedFaceCount);

    megatexturesSort(sortInfo, tmpMemory, 0, sortedFaceCount);

    for (int i = 0; i < sortedFaceCount; ++i) {
        if (!megatextureRender(tileCache, &index[sortInfo[i].index], cameraInfo, renderState)) {
            megatextureRenderEnd(tileCache, 0);
            return 0;
        }
    }

    stackMallocFree(tmpMemory);
    stackMallocFree(sortInfo);

    megatextureRenderEnd(tileCache, 1);

    return 1;
}