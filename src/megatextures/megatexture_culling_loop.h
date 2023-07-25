#ifndef __MEGATEXTURE_CULLING_LOOP_H__
#define __MEGATEXTURE_CULLING_LOOP_H__

#define MT_MAX_CULLING_LOOP_SIZE    10

#include "../math/vector2.h"
#include "../math/plane2.h"
#include "../math/plane.h"
#include "tile_index.h"
#include <ultra64.h>

struct MTCullingLoop {
    struct Vector2 loop[MT_MAX_CULLING_LOOP_SIZE];
    u16 loopSize;
};

void mtCullingLoopInit(struct MTCullingLoop* loop);
void mtCullingLoopSplit(struct MTCullingLoop* loop, struct Plane2* plane, struct MTCullingLoop* behindPlane);

int mtCullingLoopTopIndex(struct MTCullingLoop* loop);
float mtCullingLoopFindExtent(struct MTCullingLoop* loop, int* currentIndex, float* lastBoundaryExtent, float until, int direction);

void mtProjectClippingPlane(struct Plane* plane, struct MTUVBasis* uvBasis, struct Plane2* result);

#endif