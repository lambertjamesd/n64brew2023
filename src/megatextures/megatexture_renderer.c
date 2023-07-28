#include "megatexture_renderer.h"

#define MAX_VERTEX_CACHE_SIZE   32

#include "./megatexture_culling_loop.h"
#include "../math/mathf.h"
#include <math.h>
#include "../graphics/graphics.h"

#define MT_MAX_LOD              5
#define MT_MIP_SAMPLE_COUNT     3
#define MT_LOG_INV_2 1.442695041
#define MT_LOD_BIAS             0.0

#define MT_CALCULATE_MIP_LEVEL(pixelArea) (logf(1.0f / (pixelArea)) * (0.5f * MT_LOG_INV_2) + MT_LOD_BIAS)

float mtScreenSpace(struct CameraMatrixInfo* cameraInfo, struct Vector2* cameraSpacePoint) {
    return (SCREEN_HT / 2.0f) * cameraInfo->cotFov * cameraSpacePoint->x / cameraSpacePoint->y;
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
        float mipLevel = MT_CALCULATE_MIP_LEVEL(pixelArea);
        // not needed but helpful for debugging
        mipPlanesOut[0] = cameraSpacePos.y;

        *minLodOut = (int)ceilf(clampf(mipLevel, 0.0f, (float)mipPlaneCount));
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

        struct Vector2 crossTexelPos;
        vector2Add(&cameraSpacePos, &texelOffset, &crossTexelPos);
        float crossSize = fabsf(mtScreenSpace(cameraInfo, &cameraSpacePos) - mtScreenSpace(cameraInfo, &crossTexelPos));
        cameraSpacePos.x = worldPixelWidth;
        float pixelArea = mtScreenSpace(cameraInfo, &cameraSpacePos) * crossSize;

        distance[i] = cameraSpacePos.y;
        mipLevel[i] = MT_CALCULATE_MIP_LEVEL(pixelArea);

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

void megatextureRenderRow(struct MTTileCache* tileCache, struct MTTileLayer* layer, int row, int minX, int maxX, struct RenderState* renderState) {
    int currentVertexCount = 0;

    Gfx* vertexCopyCommand = renderState->dl++;

    struct MTMeshTile* tile = &layer->mesh.tiles[(row << layer->mesh.tileXBits) + minX];
    int startVertex = tile->startVertex;

    for (int x = minX; x < maxX; ++x) {
        int startIndex = tile->startVertex - startVertex;

        if (tile->vertexCount + startIndex > MAX_VERTEX_CACHE_SIZE) {
            // retroactively update the vertex command
            gSPVertex(vertexCopyCommand, &layer->mesh.vertices[startVertex], currentVertexCount, 0);
            vertexCopyCommand = renderState->dl++;
            startIndex = 0;
        }

        currentVertexCount = startIndex + tile->vertexCount;

        Gfx* tileRequest = mtTileCacheRequestTile(tileCache, &layer->tileSource[MT_TILE_WORDS * (x + row * layer->xTiles)], x, row, layer->lod);
        gSPDisplayList(renderState->dl++, tileRequest)

        u8* indices = &layer->mesh.indices[tile->startIndex];
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

        ++tile;
    }

    // retroactively update the vertex command
    gSPVertex(vertexCopyCommand, &layer->mesh.vertices[startVertex], currentVertexCount, 0);
}

void megatextureRenderLayer(struct MTTileCache* tileCache, struct MTTileLayer* layer, struct MTCullingLoop* currentLoop, float nearPlane, float farPlane, struct CameraMatrixInfo* cameraInfo, struct RenderState* renderState) {
    if (currentLoop->loopSize == 0) {
        // the plane is entirely outside the view
        return;
    }

    nearPlane *= SCENE_SCALE;
    farPlane *= SCENE_SCALE;

    cameraInfo->projectionMatrix[2][2] = (nearPlane + farPlane) / (nearPlane - farPlane);
    cameraInfo->projectionMatrix[3][2] = (2 * nearPlane * farPlane) / (nearPlane - farPlane);

    int leftIndex = mtCullingLoopTopIndex(currentLoop);
    int rightIndex = leftIndex;

    float lastLeftBoundary = currentLoop->loop[leftIndex].x;
    float lastRightBoundary = lastLeftBoundary;


    float tileStep = 1.0f / layer->yTiles;
    float nextBoundary = tileStep * (layer->mesh.minTileY + 1);

    if (layer->mesh.minTileY) {
        // update indices and last boundaries
        mtCullingLoopFindExtent(currentLoop, &leftIndex, &lastLeftBoundary, layer->mesh.minTileY * tileStep, 1);
        mtCullingLoopFindExtent(currentLoop, &rightIndex, &lastRightBoundary, layer->mesh.minTileY * tileStep, -1);
    }

    for (int row = layer->mesh.minTileY; row < layer->mesh.maxTileY; ++row, nextBoundary += tileStep) {
        float minX = mtCullingLoopFindExtent(currentLoop, &leftIndex, &lastLeftBoundary, nextBoundary, 1);
        float maxX = mtCullingLoopFindExtent(currentLoop, &rightIndex, &lastRightBoundary, nextBoundary, -1);

        if (minX == maxX) {
            // empty row
            continue;
        }


        Mtx* projection = renderStateRequestMatrices(renderState, 1);   
        guMtxF2L(cameraInfo->projectionMatrix, projection);
        gSPMatrix(renderState->dl++, projection, G_MTX_LOAD | G_MTX_PROJECTION | G_MTX_NOPUSH);
        gSPMatrix(renderState->dl++, cameraInfo->viewMtx, G_MTX_MUL | G_MTX_PROJECTION | G_MTX_NOPUSH);

        megatextureRenderRow(
            tileCache, 
            layer, 
            row, 
            MAX(layer->mesh.minTileX, (int)floorf(minX * layer->xTiles)), 
            MIN(layer->mesh.maxTileX, (int)ceilf(maxX * layer->xTiles)), 
            renderState
        );
    }
}

int mtIsBackFacing(struct CameraMatrixInfo* cameraInfo, struct MTUVBasis* basis) {
    struct Vector3 offset;
    vector3Sub(&basis->uvOrigin, &cameraInfo->cameraPosition, &offset);
    return vector3Dot(&offset, &basis->normal) >= 0.0f;
}

void megatextureRender(struct MTTileCache* tileCache, struct MTTileIndex* index, struct CameraMatrixInfo* cameraInfo, struct RenderState* renderState) {
    if (mtIsBackFacing(cameraInfo, &index->uvBasis)) {
        return;
    }

    if (isOutsideFrustrum(&cameraInfo->cullingInformation, &index->boundingBox)) {
        return;
    }

    struct MTCullingLoop cullingLoop;

    mtCullingLoopInit(&cullingLoop);
    mtCullingLoopClip(&cullingLoop, &index->uvBasis, &cameraInfo->cullingInformation);

    if (cullingLoop.loopSize == 0) {
        // the plane is entirely outside the view
        return;
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
        struct MTTileLayer* layer = &index->layers[layerIndex];

        if (layerIndex == maxLod) {
            megatextureRenderLayer(tileCache, layer, &cullingLoop, prevPlane, cameraInfo->farPlane, cameraInfo, renderState);
            break;
        }

        float clippingPlaneDistance = clipingPlaneDistances[layerIndex];
        
        struct Plane mipClippingPlane;
        mipClippingPlane.normal = cameraInfo->forwardVector;
        mipClippingPlane.d = -(vector3Dot(&mipClippingPlane.normal, &cameraInfo->cameraPosition) + clippingPlaneDistance);

        struct Plane2 clippingPlane;
        mtProjectClippingPlane(&mipClippingPlane, &index->uvBasis, &clippingPlane);

        struct MTCullingLoop currentLoop;
        mtCullingLoopSplit(&cullingLoop, &clippingPlane, &currentLoop);
        megatextureRenderLayer(tileCache, layer, &currentLoop, prevPlane, clippingPlaneDistance, cameraInfo, renderState);

        prevPlane = clippingPlaneDistance;
    }
}