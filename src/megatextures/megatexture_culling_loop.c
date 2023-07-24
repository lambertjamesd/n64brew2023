#include "megatexture_culling_loop.h"

#include "../util/memory.h"
#include "../math/mathf.h"

struct Vector2 gStartingLoop[] = {
    {0.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f},
    {1.0f, 0.0f},
};

void mtCullingLoopInit(struct MTCullingLoop* loop) {
    memCopy(loop->loop, &gStartingLoop, sizeof(gStartingLoop));
    loop->loopSize = 4;
}

void mtCullingLoopAdd(struct MTCullingLoop* loop, struct Vector2* point) {
    if (!loop) {
        return;
    }

    loop->loop[loop->loopSize] = *point;
    ++loop->loopSize;
}

void mtCullingLoopSplit(struct MTCullingLoop* loop, struct Plane2* plane, struct MTCullingLoop* behindPlane) {
    struct MTCullingLoop frontPlane;

    if (behindPlane) {
        behindPlane->loopSize = 0;
    }
    frontPlane.loopSize = 0;

    for (int i = 0; i < loop->loopSize; ++i) {
        struct Vector2* currentPoint = &loop->loop[i];
        struct Vector2* nextPoint = &loop->loop[(i == loop->loopSize - 1) ? 0 : i + 1];

        float currentDistance = plane2DistanceToPoint(plane, currentPoint);

        if (fabsf(currentDistance) < 0.00001f) {
            mtCullingLoopAdd(behindPlane, currentPoint);
            mtCullingLoopAdd(&frontPlane, currentPoint);
            continue;
        }

        float nextDistance = plane2DistanceToPoint(plane, nextPoint);

        if (fabsf(currentDistance) < 0.00001f) {
            // point will be added to next iteration
            continue;
        }

        if (currentDistance < 0.0f) {
            mtCullingLoopAdd(behindPlane, currentPoint);
        } else {
            mtCullingLoopAdd(&frontPlane, currentPoint);
        }

        if (currentDistance * nextDistance < 0.0f) {
            float totalDistance = currentDistance - nextDistance;
            struct Vector2 clipPoint;
            vector2Lerp(currentPoint, nextPoint, currentDistance / totalDistance, &clipPoint);
            mtCullingLoopAdd(behindPlane, &clipPoint);
            mtCullingLoopAdd(&frontPlane, &clipPoint);
        }
    }

    // copy back into the loop
    for (int i = 0; i < frontPlane.loopSize; ++i) {
        loop->loop[i] = frontPlane.loop[i];
    }
    loop->loopSize = frontPlane.loopSize;
}

void mtProjectClippingPlane(struct Plane* plane, struct MTUVBasis* uvBasis, struct Plane2* result) {
    result->normal.x = vector3Dot(&plane->normal, &uvBasis->uvRight);
    result->normal.y = vector3Dot(&plane->normal, &uvBasis->uvUp);
    result->d = plane->d + vector3Dot(&plane->normal, &uvBasis->uvOrigin);
}