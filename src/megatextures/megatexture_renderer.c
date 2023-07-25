#include "megatexture_renderer.h"

#define MAX_VERTEX_CACHE_SIZE   32

#include "./megatexture_culling_loop.h"
#include "../math/mathf.h"

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

void megatextureRender(struct MTTileCache* tileCache, struct MTTileIndex* index, struct FrustrumCullingInformation* cullingPlanes, struct RenderState* renderState) {
    if (isOutsideFrustrum(cullingPlanes, &index->boundingBox)) {
        return;
    }

    struct MTCullingLoop cullingLoop;

    mtCullingLoopInit(&cullingLoop);

    for (int i = 0; i < cullingPlanes->usedClippingPlaneCount; ++i) {
        struct Plane2 clippingPlane;
        mtProjectClippingPlane(&cullingPlanes->clippingPlanes[i], &index->uvBasis, &clippingPlane);

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

    int leftIndex = mtCullingLoopTopIndex(&cullingLoop);
    int rightIndex = leftIndex;

    float lastLeftBoundary = cullingLoop.loop[leftIndex].x;
    float lastRightBoundary = lastLeftBoundary;

    struct MTTileLayer* topLayer = &index->layers[0];

    float tileStep = 1.0f / topLayer->yTiles;
    float nextBoundary = tileStep * (topLayer->mesh.minTileY + 1);

    if (topLayer->mesh.minTileY) {
        mtCullingLoopFindExtent(&cullingLoop, &leftIndex, &lastLeftBoundary, topLayer->mesh.minTileY * tileStep, 1);
        mtCullingLoopFindExtent(&cullingLoop, &rightIndex, &lastRightBoundary, topLayer->mesh.minTileY * tileStep, -1);
    }

    for (int row = topLayer->mesh.minTileY; row < topLayer->mesh.maxTileY; ++row, nextBoundary += tileStep) {
        float minX = mtCullingLoopFindExtent(&cullingLoop, &leftIndex, &lastLeftBoundary, nextBoundary, 1);
        float maxX = mtCullingLoopFindExtent(&cullingLoop, &rightIndex, &lastRightBoundary, nextBoundary, -1);

        if (minX == maxX) {
            // empty row
            continue;
        }

        megatextureRenderRow(
            tileCache, 
            topLayer, 
            row, 
            MAX(topLayer->mesh.minTileX, (int)floorf(minX * topLayer->xTiles)), 
            MIN(topLayer->mesh.maxTileX, (int)ceilf(maxX * topLayer->xTiles)), 
            renderState
        );
    }
}