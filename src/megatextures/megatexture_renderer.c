#include "megatexture_renderer.h"

#define MAX_VERTEX_CACHE_SIZE   32

#include "./megatexture_culling_loop.h"
#include "../math/mathf.h"
#include "../graphics/graphics.h"

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

float megatextureDetermineMipBoundary(struct CameraMatrixInfo* cameraInfo, struct MTUVBasis* basis, float worldPixelSize, float targetScreenSize) {
    return cameraInfo->cotFov * (0.5f * SCREEN_HT) * worldPixelSize / targetScreenSize;
}

void megatextureRender(struct MTTileCache* tileCache, struct MTTileIndex* index, struct CameraMatrixInfo* cameraInfo, struct RenderState* renderState) {
    if (isOutsideFrustrum(&cameraInfo->cullingInformation, &index->boundingBox)) {
        return;
    }

    struct MTCullingLoop cullingLoop;

    mtCullingLoopInit(&cullingLoop);

    for (int i = 0; i < cameraInfo->cullingInformation.usedClippingPlaneCount; ++i) {
        struct Plane2 clippingPlane;
        mtProjectClippingPlane(&cameraInfo->cullingInformation.clippingPlanes[i], &index->uvBasis, &clippingPlane);

        if (fabsf(clippingPlane.normal.x) < 0.000001f && fabsf(clippingPlane.normal.y) < 0.000001f) {
            if (clippingPlane.d < 0.0f) {
                // the plane is entirely outside the view
                return;
            }

            // the plane is entirely inside the view
            continue;
        }

        mtCullingLoopSplit(&cullingLoop, &clippingPlane, NULL);

        if (cullingLoop.loopSize == 0) {
            // the plane is entirely outside the view
            return;
        }
    }

    float prevPlane = cameraInfo->nearPlane;

    for (int layerIndex = 0; layerIndex < index->layerCount; ++layerIndex) {
        struct MTTileLayer* layer = &index->layers[layerIndex];

        float clippingPlaneDistance = megatextureDetermineMipBoundary(cameraInfo, &index->uvBasis, layer->worldPixelSize, 1.0f);
        
        struct Plane mipClippingPlane;
        mipClippingPlane.normal = cameraInfo->forwardVector;
        mipClippingPlane.d = -SCENE_SCALE * (vector3Dot(&mipClippingPlane.normal, &cameraInfo->cameraPosition) + clippingPlaneDistance);

        struct Plane2 clippingPlane;
        mtProjectClippingPlane(&mipClippingPlane, &index->uvBasis, &clippingPlane);

        struct MTCullingLoop currentLoop;

        if (fabsf(clippingPlane.normal.x) < 0.000001f && fabsf(clippingPlane.normal.y) < 0.000001f) {
            if (clippingPlane.d < 0.0f && layerIndex < index->layerCount - 1) {
                continue;
            }

            currentLoop = cullingLoop;
            cullingLoop.loopSize = 0;
        } else {
            mtCullingLoopSplit(&cullingLoop, &clippingPlane, &currentLoop);
        }

        if (clippingPlaneDistance <= prevPlane && layerIndex < index->layerCount - 1) {
            continue;
        }

        float nearPlane = prevPlane * SCENE_SCALE;
        float farPlane = clippingPlaneDistance * SCENE_SCALE;

        cameraInfo->projectionMatrix[2][2] = (nearPlane + farPlane) / (nearPlane - farPlane);
        cameraInfo->projectionMatrix[3][2] = (2 * nearPlane * farPlane) / (nearPlane - farPlane);

        prevPlane = clippingPlaneDistance;

        if (currentLoop.loopSize == 0) {
            // the plane is entirely outside the view
            continue;;
        }

        int leftIndex = mtCullingLoopTopIndex(&currentLoop);
        int rightIndex = leftIndex;

        float lastLeftBoundary = currentLoop.loop[leftIndex].x;
        float lastRightBoundary = lastLeftBoundary;


        float tileStep = 1.0f / layer->yTiles;
        float nextBoundary = tileStep * (layer->mesh.minTileY + 1);

        if (layer->mesh.minTileY) {
            mtCullingLoopFindExtent(&currentLoop, &leftIndex, &lastLeftBoundary, layer->mesh.minTileY * tileStep, 1);
            mtCullingLoopFindExtent(&currentLoop, &rightIndex, &lastRightBoundary, layer->mesh.minTileY * tileStep, -1);
        }

        for (int row = layer->mesh.minTileY; row < layer->mesh.maxTileY; ++row, nextBoundary += tileStep) {
            float minX = mtCullingLoopFindExtent(&currentLoop, &leftIndex, &lastLeftBoundary, nextBoundary, 1);
            float maxX = mtCullingLoopFindExtent(&currentLoop, &rightIndex, &lastRightBoundary, nextBoundary, -1);

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
}